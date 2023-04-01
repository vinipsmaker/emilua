#include <emilua/linux_namespaces.hpp>

#include <sys/prctl.h>
#include <sys/wait.h>

#include <linux/close_range.h>

#include <iostream>
#include <charconv>

#include <fmt/ostream.h>
#include <fmt/format.h>

#include <boost/serialization/unordered_map.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/preprocessor/stringize.hpp>
#include <boost/nowide/iostream.hpp>
#include <boost/scope_exit.hpp>
#include <boost/hana/less.hpp>
#include <boost/hana/plus.hpp>

#include <emilua/file_descriptor.hpp>
#include <emilua/actor.hpp>
#include <emilua/state.hpp>

#define EMILUA_LUA_HOOK_BUFFER_SIZE (1024 * 1024)
static_assert(EMILUA_LUA_HOOK_BUFFER_SIZE % alignof(std::max_align_t) == 0);

namespace emilua {

namespace fs = std::filesystem;

int posix_mt_index(lua_State* L);

static int inboxfd;
static int proc_stdin;
static int proc_stdout;
static int proc_stderr;
static bool proc_stderr_has_color;
static bool has_lua_hook;

struct monotonic_allocator
{
    monotonic_allocator(void* buffer, std::size_t buffer_size)
        : buffer{static_cast<char*>(buffer)}
        , next{static_cast<char*>(buffer)}
        , buffer_size{buffer_size}
    {}

    void* operator()(void* ptr, std::size_t osize, std::size_t nsize)
    {
        static constexpr auto step_size = alignof(std::max_align_t);

        if (nsize == 0)
            return NULL;
        else if (nsize < osize)
            return ptr;

        auto end = buffer + buffer_size;

        if (auto remainder = nsize % step_size ; remainder > 0) {
            auto padding = step_size - remainder;
            nsize += padding;
        }

        if (next + nsize > end) {
            errno = ENOMEM;
            perror("<3>linux_namespaces/init/alloc");
            std::exit(1);
        }

        auto ret = next;
        next += nsize;

        if (ptr)
            std::memcpy(ret, ptr, osize);

        return ret;
    }

    char* buffer;
    char* next;
    std::size_t buffer_size;

    static void* alloc(void* ud, void* ptr, std::size_t osize,
                       std::size_t nsize)
    {
        return (*static_cast<monotonic_allocator*>(ud))(ptr, osize, nsize);
    }
};

static int receive_with_fd(lua_State* L)
{
    int fd = luaL_checkinteger(L, 1);
    int nbyte = luaL_checkinteger(L, 2);
    void* ud;
    lua_Alloc a = lua_getallocf(L, &ud);
    char* buf = static_cast<char*>(a(ud, NULL, 0, nbyte));

    struct msghdr msg;
    std::memset(&msg, 0, sizeof(msg));

    struct iovec iov;
    iov.iov_base = buf;
    iov.iov_len = nbyte;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    union
    {
        struct cmsghdr align;
        char buf[CMSG_SPACE(sizeof(int))];
    } cmsgu;
    msg.msg_control = cmsgu.buf;
    msg.msg_controllen = sizeof(cmsgu.buf);

    int res = recvmsg(fd, &msg, MSG_CMSG_CLOEXEC);
    int last_error = (res == -1) ? errno : 0;
    if (last_error != 0) {
        lua_getfield(L, LUA_GLOBALSINDEX, "errexit");
        if (lua_toboolean(L, -1)) {
            errno = last_error;
            perror("<3>linux_namespaces/init");
            std::exit(1);
        }
    }
    if (last_error == 0) {
        lua_pushlstring(L, buf, res);
    } else {
        lua_pushnil(L);
    }

    int fd_received = -1;
    for (struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg) ; cmsg != NULL ;
         cmsg = CMSG_NXTHDR(&msg, cmsg)) {
        if (cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS)
            continue;

        std::memcpy(&fd_received, CMSG_DATA(cmsg), sizeof(int));
        break;
    }
    lua_pushinteger(L, fd_received);

    lua_pushinteger(L, last_error);

    return 3;
}

static int send_with_fd(lua_State* L)
{
    int fd = luaL_checkinteger(L, 1);
    std::size_t len;
    const char* str = lua_tolstring(L, 2, &len);
    int fd_to_send = luaL_checkinteger(L, 3);

    struct msghdr msg;
    std::memset(&msg, 0, sizeof(msg));

    struct iovec iov;
    iov.iov_base = const_cast<char*>(str);
    iov.iov_len = len;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    union {
        struct cmsghdr align;
        char buf[CMSG_SPACE(sizeof(int))];
    } cmsgu;
    msg.msg_control = cmsgu.buf;
    msg.msg_controllen = CMSG_SPACE(sizeof(int));
    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    std::memcpy(CMSG_DATA(cmsg), &fd_to_send, sizeof(int));

    int res = sendmsg(fd, &msg, MSG_NOSIGNAL);
    int last_error = (res == -1) ? errno : 0;
    if (last_error != 0) {
        lua_getfield(L, LUA_GLOBALSINDEX, "errexit");
        if (lua_toboolean(L, -1)) {
            errno = last_error;
            perror("<3>linux_namespaces/init");
            std::exit(1);
        }
    }
    lua_pushinteger(L, res);
    lua_pushinteger(L, last_error);
    return 2;
}

static void init_hookstate(lua_State* L)
{
    lua_pushboolean(L, 1);
    lua_setglobal(L, "errexit");

    lua_newtable(L);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/1);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, posix_mt_index);
        lua_rawset(L, -3);

        lua_setmetatable(L, -2);
    }
    lua_setglobal(L, "C");

    lua_pushcfunction(L, receive_with_fd);
    lua_setglobal(L, "receive_with_fd");

    lua_pushcfunction(L, send_with_fd);
    lua_setglobal(L, "send_with_fd");

    lua_pushcfunction(
        L,
        [](lua_State* L) -> int {
            mode_t u = luaL_checkinteger(L, 1);
            mode_t g = luaL_checkinteger(L, 2);
            mode_t o = luaL_checkinteger(L, 3);
            lua_pushinteger(L, (u << 6) | (g << 3) | o);
            return 1;
        });
    lua_setglobal(L, "mode");
}

void linux_container_inbox_op::do_wait()
{
    service->sock.async_wait(
        asio::socket_base::wait_read,
        asio::bind_executor(
            remap_post_to_defer<strand_type>{executor},
            [self=shared_from_this()](const boost::system::error_code& ec) {
                self->on_wait(ec);
            }
        )
    );
}

void linux_container_inbox_op::on_wait(const boost::system::error_code& ec)
{
    auto vm_ctx = this->vm_ctx.lock();
    if (!vm_ctx || !vm_ctx->valid())
        return;

    if (!vm_ctx->inbox.open) {
        --vm_ctx->inbox.nsenders;
        vm_ctx->pending_operations.erase(
            vm_ctx->pending_operations.iterator_to(*service));
        delete service;
        return;
    }

    auto recv_fiber = vm_ctx->inbox.recv_fiber;
    if (ec) {
        vm_ctx->pending_operations.erase(
            vm_ctx->pending_operations.iterator_to(*service));
        delete service;
        if (--vm_ctx->inbox.nsenders == 0 && recv_fiber) {
            vm_ctx->inbox.recv_fiber = nullptr;
            vm_ctx->inbox.work_guard.reset();
            vm_ctx->fiber_resume(
                recv_fiber,
                hana::make_set(
                    hana::make_pair(
                        vm_context::options::arguments,
                        hana::make_tuple(errc::no_senders))));
        }
        return;
    }

    struct msghdr msg;
    std::memset(&msg, 0, sizeof(msg));

    struct iovec iov;
    linux_container_message message;
    iov.iov_base = &message;
    iov.iov_len = sizeof(message);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    union
    {
        struct cmsghdr align;
        char buf[CMSG_SPACE(
            sizeof(int) *
            EMILUA_CONFIG_LINUX_NAMESPACES_MESSAGE_MAX_MEMBERS_NUMBER)];
    } cmsgu;
    msg.msg_control = cmsgu.buf;
    msg.msg_controllen = sizeof(cmsgu.buf);

    auto nread = recvmsg(service->sock.native_handle(), &msg, MSG_DONTWAIT);
    if (nread == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            do_wait();
            return;
        }

        vm_ctx->pending_operations.erase(
            vm_ctx->pending_operations.iterator_to(*service));
        delete service;
        if (--vm_ctx->inbox.nsenders == 0 && recv_fiber) {
            vm_ctx->inbox.recv_fiber = nullptr;
            vm_ctx->inbox.work_guard.reset();
            vm_ctx->fiber_resume(
                recv_fiber,
                hana::make_set(
                    hana::make_pair(
                        vm_context::options::arguments,
                        hana::make_tuple(errc::no_senders))));
        }
        return;
    }

    std::vector<int> fds;
    fds.reserve(EMILUA_CONFIG_LINUX_NAMESPACES_MESSAGE_MAX_MEMBERS_NUMBER);
    BOOST_SCOPE_EXIT_ALL(&) {
        for (auto& fd: fds) {
            if (fd != -1) {
                int res = close(fd);
                boost::ignore_unused(res);
            }
        }
    };
    for (struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg) ; cmsg != NULL ;
         cmsg = CMSG_NXTHDR(&msg, cmsg)) {
        if (cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS) {
            continue;
        }

        char* in = (char*)CMSG_DATA(cmsg);
        auto nfds = (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int);
        for (std::size_t i = 0 ; i != nfds ; ++i) {
            int fd;
            std::memcpy(&fd, in, sizeof(int));
            in += sizeof(int);
            if (fd != -1)
                fds.emplace_back(fd);
        }
    }

    if (
        nread < static_cast<ssize_t>(sizeof(message.members[0]) * 2) ||
        (msg.msg_flags & (MSG_TRUNC | MSG_CTRUNC))
    ) {
        vm_ctx->pending_operations.erase(
            vm_ctx->pending_operations.iterator_to(*service));
        delete service;
        if (--vm_ctx->inbox.nsenders == 0 && recv_fiber) {
            vm_ctx->inbox.recv_fiber = nullptr;
            vm_ctx->inbox.work_guard.reset();
            vm_ctx->fiber_resume(
                recv_fiber,
                hana::make_set(
                    hana::make_pair(
                        vm_context::options::arguments,
                        hana::make_tuple(errc::no_senders))));
        }
        return;
    }
    service->running = false;

    auto remove_service = [&]() {
        // Ideally we wouldn't close the channel because the assumption is that
        // many writers have this handle and only one is misbehaving. Closing
        // the channel would cut our communication channel with the legitimate
        // parties which really means DoS.
        //
        // Unfortunately a misbehaving writer can just send a 0-sized payload
        // and trick us that EOF was reached anyway. Therefore keeping the
        // connection open just to defend against such DoS attacker would be
        // moot. Let's go ahead and just close the socket already.
        vm_ctx->pending_operations.erase(
            vm_ctx->pending_operations.iterator_to(*service));
        delete service;
        --vm_ctx->inbox.nsenders;
    };

    auto boundsvalid = [&message,&nread](const std::string_view& v) {
        // No overflow ever happens as it's impossible to overrun our buffer
        // with its current encoding scheme. The point is to avoid leaking
        // uninitialized data from our stack. That's the attack we're defending
        // against.
        return v.data() + v.size() <= reinterpret_cast<char*>(&message) + nread;
    };

    if (!recv_fiber) {
        auto& queue = vm_ctx->inbox.incoming;
        queue.emplace_back(std::nullopt);

        if (
            message.members[0].as_int ==
            (EXPONENT_MASK | linux_container_message::nil)
        ) {
            if (!is_snan(message.members[1].as_int)) {
                queue.back().msg.emplace<lua_Number>(
                    message.members[1].as_double);
                return;
            }

            switch (message.members[1].as_int & MANTISSA_MASK) {
            default:
                remove_service();
                queue.pop_back();
                return;
            case linux_container_message::boolean_true:
                queue.back().msg.emplace<bool>(true);
                break;
            case linux_container_message::boolean_false:
                queue.back().msg.emplace<bool>(false);
                break;
            case linux_container_message::string: {
                std::string_view v(reinterpret_cast<char*>(message.strbuf) + 1,
                                   *message.strbuf);
                if (!boundsvalid(v)) {
                    remove_service();
                    queue.pop_back();
                    return;
                }
                queue.back().msg.emplace<std::string>(v);
                break;
            }
            case linux_container_message::file_descriptor: {
                if (fds.size() != 1) {
                    remove_service();
                    queue.pop_back();
                    return;
                }

                queue.back().msg.emplace<
                    std::shared_ptr<inbox_t::file_descriptor_box>
                >(std::make_shared<inbox_t::file_descriptor_box>(fds[0]));
                fds[0] = -1;
                break;
            }
            case linux_container_message::actor_address:
                if (fds.size() != 1) {
                    remove_service();
                    queue.pop_back();
                    return;
                }

                queue.back().msg.emplace<inbox_t::linux_container_address>(
                    std::make_shared<inbox_t::file_descriptor_box>(fds[0]));
                fds[0] = -1;
                break;
            }
            return;
        }

        if (nread < static_cast<ssize_t>(sizeof(message.members))) {
            remove_service();
            queue.pop_back();
            return;
        }

        auto nextstr = [strit=message.strbuf]() mutable {
            std::string_view::size_type size = *strit++;
            std::string_view ret(reinterpret_cast<char*>(strit), size);
            strit += size;
            return ret;
        };
        auto& dict = queue.back().msg.emplace<
            std::map<std::string, inbox_t::value_type>>();

        decltype(fds)::size_type fdsidx = 0;
        for (
            int nf = 0 ;
            nf != EMILUA_CONFIG_LINUX_NAMESPACES_MESSAGE_MAX_MEMBERS_NUMBER ;
            ++nf
        ) {
            if (
                message.members[nf].as_int ==
                (EXPONENT_MASK | linux_container_message::nil)
            ) {
                assert(nf > 0);
                break;
            }

            auto key = nextstr();
            if (!boundsvalid(key)) {
                remove_service();
                queue.pop_back();
                return;
            }

            if (!is_snan(message.members[nf].as_int)) {
                dict.emplace(key, lua_Number(message.members[nf].as_double));
                continue;
            }

            switch (message.members[nf].as_int & MANTISSA_MASK) {
            default:
                remove_service();
                queue.pop_back();
                return;
            case linux_container_message::boolean_true:
                dict.emplace(key, true);
                break;
            case linux_container_message::boolean_false:
                dict.emplace(key, false);
                break;
            case linux_container_message::string: {
                auto value = nextstr();
                if (!boundsvalid(value)) {
                    remove_service();
                    queue.pop_back();
                    return;
                }
                dict.emplace(key, static_cast<std::string>(value));
                break;
            }
            case linux_container_message::file_descriptor:
                if (fdsidx == fds.size()) {
                    remove_service();
                    queue.pop_back();
                    return;
                }

                dict.emplace(
                    key,
                    std::make_shared<inbox_t::file_descriptor_box>(
                        fds[fdsidx]));
                fds[fdsidx++] = -1;
                break;
            case linux_container_message::actor_address:
                if (fdsidx == fds.size()) {
                    remove_service();
                    queue.pop_back();
                    return;
                }

                dict.emplace(
                    key,
                    inbox_t::linux_container_address{
                        std::make_shared<inbox_t::file_descriptor_box>(
                            fds[fdsidx])});
                fds[fdsidx++] = -1;
                break;
            }
        }
        return;
    }

    struct bad_message_t {};

    auto checkbounds = [&boundsvalid](const std::string_view& v) {
        if (!boundsvalid(v)) {
            throw bad_message_t{};
        }
    };

    auto deserializer = [&message,&checkbounds,&fds,nread,this](
        lua_State* recv_fiber
    ) {
        if (
            message.members[0].as_int ==
            (EXPONENT_MASK | linux_container_message::nil)
        ) {
            if (!is_snan(message.members[1].as_int)) {
                lua_pushnumber(recv_fiber, message.members[1].as_double);
                return;
            }

            switch (message.members[1].as_int & MANTISSA_MASK) {
            default:
                throw bad_message_t{};
            case linux_container_message::boolean_true:
                lua_pushboolean(recv_fiber, 1);
                break;
            case linux_container_message::boolean_false:
                lua_pushboolean(recv_fiber, 0);
                break;
            case linux_container_message::string: {
                std::string_view v(reinterpret_cast<char*>(message.strbuf) + 1,
                                   *message.strbuf);
                checkbounds(v);
                push(recv_fiber, v);
                break;
            }
            case linux_container_message::file_descriptor: {
                if (fds.size() != 1) {
                    throw bad_message_t{};
                }

                auto fdhandle = static_cast<file_descriptor_handle*>(
                    lua_newuserdata(recv_fiber, sizeof(file_descriptor_handle))
                );
                rawgetp(recv_fiber, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
                setmetatable(recv_fiber, -2);
                *fdhandle = fds[0];
                fds[0] = -1;
                break;
            }
            case linux_container_message::actor_address:
                if (fds.size() != 1) {
                    throw bad_message_t{};
                }

                auto ch = static_cast<linux_container_address*>(
                    lua_newuserdata(recv_fiber, sizeof(linux_container_address))
                );
                rawgetp(recv_fiber, LUA_REGISTRYINDEX,
                        &linux_container_chan_mt_key);
                setmetatable(recv_fiber, -2);
                new (ch) linux_container_address{executor.context()};
                {
                    asio::local::datagram_protocol protocol;
                    boost::system::error_code ignored_ec;
                    ch->dest.assign(protocol, fds[0], ignored_ec);
                    assert(!ignored_ec);
                    fds[0] = -1;
                }
                break;
            }
            return;
        }

        if (nread < static_cast<ssize_t>(sizeof(message.members))) {
            throw bad_message_t{};
        }

        auto nextstr = [strit=message.strbuf,&checkbounds]() mutable {
            std::string_view::size_type size = *strit++;
            std::string_view ret(reinterpret_cast<char*>(strit), size);
            strit += size;
            checkbounds(ret);
            return ret;
        };
        lua_newtable(recv_fiber);

        decltype(fds)::size_type fdsidx = 0;
        for (
            int nf = 0 ;
            nf != EMILUA_CONFIG_LINUX_NAMESPACES_MESSAGE_MAX_MEMBERS_NUMBER ;
            ++nf
        ) {
            if (
                message.members[nf].as_int ==
                (EXPONENT_MASK | linux_container_message::nil)
            ) {
                assert(nf > 0);
                break;
            }

            auto key = nextstr();
            push(recv_fiber, key);

            if (!is_snan(message.members[nf].as_int)) {
                lua_pushnumber(recv_fiber, message.members[nf].as_double);
                lua_rawset(recv_fiber, -3);
                continue;
            }

            switch (message.members[nf].as_int & MANTISSA_MASK) {
            default:
                throw bad_message_t{};
            case linux_container_message::boolean_true:
                lua_pushboolean(recv_fiber, 1);
                break;
            case linux_container_message::boolean_false:
                lua_pushboolean(recv_fiber, 0);
                break;
            case linux_container_message::string:
                push(recv_fiber, nextstr());
                break;
            case linux_container_message::file_descriptor: {
                if (fdsidx == fds.size()) {
                    throw bad_message_t{};
                }

                auto fdhandle = static_cast<file_descriptor_handle*>(
                    lua_newuserdata(recv_fiber, sizeof(file_descriptor_handle))
                );
                rawgetp(recv_fiber, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
                setmetatable(recv_fiber, -2);
                *fdhandle = fds[fdsidx];
                fds[fdsidx++] = -1;
                break;
            }
            case linux_container_message::actor_address:
                if (fdsidx == fds.size()) {
                    throw bad_message_t{};
                }

                auto ch = static_cast<linux_container_address*>(
                    lua_newuserdata(recv_fiber, sizeof(linux_container_address))
                );
                rawgetp(recv_fiber, LUA_REGISTRYINDEX,
                        &linux_container_chan_mt_key);
                setmetatable(recv_fiber, -2);
                new (ch) linux_container_address{executor.context()};
                {
                    asio::local::datagram_protocol protocol;
                    boost::system::error_code ignored_ec;
                    ch->dest.assign(protocol, fds[fdsidx], ignored_ec);
                    assert(!ignored_ec);
                    fds[fdsidx++] = -1;
                }
                break;
            }
            lua_rawset(recv_fiber, -3);
        }
    };

    vm_ctx->inbox.recv_fiber = nullptr;
    vm_ctx->inbox.work_guard.reset();
    try {
        vm_ctx->fiber_resume(
            recv_fiber,
            hana::make_set(
                hana::make_pair(
                    vm_context::options::arguments,
                    hana::make_tuple(std::nullopt, deserializer))));
    } catch (const bad_message_t&) {
        remove_service();
        if (vm_ctx->inbox.nsenders == 0) {
            vm_ctx->fiber_resume(
                recv_fiber,
                hana::make_set(
                    hana::make_pair(
                        vm_context::options::arguments,
                        hana::make_tuple(errc::no_senders))));
        } else {
            vm_ctx->inbox.recv_fiber = recv_fiber;
            vm_ctx->inbox.work_guard = vm_ctx;
        }
    }
}

static int child_main(void*)
{
    switch (proc_stdin) {
    case -1: {
        int pipefd[2];
        if (pipe(pipefd) == -1)
            return 1;
        if (dup2(pipefd[0], STDIN_FILENO) == -1)
            return 1;
        break;
    }
    case STDIN_FILENO:
        break;
    default:
        if (dup2(proc_stdin, STDIN_FILENO) == -1)
            return 1;
    }

    switch (proc_stdout) {
    case -1: {
        int pipefd[2];
        if (pipe(pipefd) == -1)
            return 1;
        if (dup2(pipefd[1], STDOUT_FILENO) == -1)
            return 1;
        break;
    }
    case STDOUT_FILENO:
        break;
    default:
        if (dup2(proc_stdout, STDOUT_FILENO) == -1)
            return 1;
    }

    switch (proc_stderr) {
    case -1: {
        int pipefd[2];
        if (pipe(pipefd) == -1)
            return 1;
        if (dup2(pipefd[1], STDERR_FILENO) == -1)
            return 1;
        break;
    }
    case STDERR_FILENO:
        break;
    default:
        if (dup2(proc_stderr, STDERR_FILENO) == -1)
            return 1;
    }

    if (dup2(inboxfd, 3) == -1)
        return 1;
    inboxfd = 3;

    {
        int oldflags = fcntl(inboxfd, F_GETFD);
        assert(oldflags != -1);
        if (fcntl(inboxfd, F_SETFD, oldflags | FD_CLOEXEC) == -1)
            return 1;
    }

    if (close_range(4, UINT_MAX, /*flags=*/0) == -1)
        return 1;

    {
        struct sigaction sa;
        sa.sa_handler = SIG_DFL;
        sigemptyset(&sa.sa_mask);
        // buggy CLONE_CLEAR_SIGHAND won't clear sa_flags so we do it manually
        sa.sa_flags = 0;
        sigaction(SIGCHLD, /*act=*/&sa, /*oldact=*/NULL);

        sigset_t set;
        sigfillset(&set);
        sigdelset(&set, SIGCHLD);
        sigprocmask(SIG_UNBLOCK, &set, /*oldset=*/NULL);
    }

    // The parent-death signal is a completely voluntary ritual and an untrusted
    // guest can just disable it later on. However we don't depend on
    // parent-death signals. We install a parent-death signal merely as a
    // convenience for bad sysadmins that forcefully kill the supervisor
    // processor (an unsafe operation in itself). The parent-death signal
    // setting is also cleared upon changes to effective user ID, effective
    // group ID, filesystem user ID, or filesystem group ID.
    prctl(PR_SET_PDEATHSIG, SIGKILL);

    std::string buffer;
    buffer.resize(EMILUA_CONFIG_LINUX_NAMESPACES_MESSAGE_SIZE);

    {
        auto nread = read(inboxfd, buffer.data(), buffer.size());
        if (nread == -1 || nread == 0)
            return 1;

        // the supervisor process writes in the same fd so we can only shutdown
        // it upon reaching the sync point which happens to be the receipt of
        // the first message
        if (shutdown(inboxfd, SHUT_WR) == -1)
            return 1;

        buffer.resize(nread);
    }

    if (has_lua_hook) {
        monotonic_allocator allocator{
            malloc(EMILUA_LUA_HOOK_BUFFER_SIZE), EMILUA_LUA_HOOK_BUFFER_SIZE};
        BOOST_SCOPE_EXIT_ALL(&) {
            explicit_bzero(allocator.buffer, allocator.next - allocator.buffer);
            free(allocator.buffer);
        };

        struct msghdr msg;
        std::memset(&msg, 0, sizeof(msg));

        struct iovec iov;
        iov.iov_base = allocator.buffer;
        iov.iov_len = allocator.buffer_size;
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;

        union
        {
            struct cmsghdr align;
            char buf[CMSG_SPACE(sizeof(int))];
        } cmsgu;
        msg.msg_control = cmsgu.buf;
        msg.msg_controllen = sizeof(cmsgu.buf);

        auto nread = recvmsg(inboxfd, &msg, MSG_CMSG_CLOEXEC);
        if (
            nread == -1 || nread == 0 ||
            (msg.msg_flags & (MSG_TRUNC | MSG_CTRUNC)) ||
            allocator(NULL, 0, nread) == NULL ||
            allocator.next == allocator.buffer + allocator.buffer_size
        ) {
            return 1;
        }

        int fdarg = -1;
        for (struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg) ; cmsg != NULL ;
             cmsg = CMSG_NXTHDR(&msg, cmsg)) {
            if (cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS)
                continue;

            std::memcpy(&fdarg, CMSG_DATA(cmsg), sizeof(int));
            break;
        }

        lua_State* L = lua_newstate(allocator.alloc, &allocator);
        BOOST_SCOPE_EXIT_ALL(&) { lua_close(L); };
        lua_gc(L, LUA_GCSTOP, /*unused_data=*/0);
        luaL_openlibs(L);
        init_hookstate(L);

        if (fdarg != -1) {
            lua_pushinteger(L, fdarg);
            lua_setfield(L, LUA_GLOBALSINDEX, "fdarg");
        }

        if (luaL_loadbuffer(L, allocator.buffer, nread, NULL) != 0)
            return 1;
        if (lua_pcall(L, /*nargs=*/0, /*nresults=*/0, /*errfunc=*/0) != 0) {
            if (lua_type(L, -1) == LUA_TSTRING) {
                auto prefix = "<3>linux_namespaces/init/script: "sv;
                auto from_lua = tostringview(L);
                std::string error_string;
                error_string.reserve(prefix.size() + from_lua.size() + 1);
                error_string += prefix;
                error_string += from_lua;
                error_string += '\n';
                std::cerr << error_string;
            }
            return 1;
        }

        if (close_range(4, UINT_MAX, /*flags=*/0) == -1)
            return 1;
    }

    if (getpid() == 1) {
        static pid_t childpid;

        static constexpr auto sighandler = [](int signo) {
            // upon reaping childpid, there's a small window for an ESRCH
            // race... and that doesn't matter because as soon as child finishes
            // the whole pidns is going down
            kill(childpid, signo);
        };

        switch (childpid = fork()) {
        case -1:
            return 1;
        case 0:
            break;
        default: {
            struct sigaction sa;
            sa.sa_handler = sighandler;
            sigemptyset(&sa.sa_mask);
            sa.sa_flags = 0;

            // It'd be futile to re-route every signal (e.g. SIGSTOP) so we
            // don't even try it. Only re-route signals that are useful for
            // parent-child communication.
            sigaction(SIGTERM, /*act=*/&sa, /*oldact=*/NULL);
            sigaction(SIGUSR1, /*act=*/&sa, /*oldact=*/NULL);
            sigaction(SIGUSR2, /*act=*/&sa, /*oldact=*/NULL);

            // Applications that don't require a controlling terminal, such as
            // daemons, usually re-purpose SIGHUP as a signal to re-read
            // configuration files.
            sigaction(SIGHUP, /*act=*/&sa, /*oldact=*/NULL);

            // SIGINT shouldn't be the first choice for parent-child
            // communication. SIGINT is generated by the TTY driver itself and
            // as such means UI running in the interested process communicated
            // an UI-initiated interruption request (e.g. confirmation dialogues
            // are allowed). However we also re-route SIGINT because
            // LINUX_REBOOT_CMD_CAD_OFF (again: an UI interaction) will send
            // SIGINT to PID1.
            sigaction(SIGINT, /*act=*/&sa, /*oldact=*/NULL);

            for (siginfo_t info ;;) {
                waitid(P_ALL, /*ignored_id=*/0, &info, WEXITED);
                if (info.si_pid == childpid) {
                    if (info.si_code == CLD_EXITED) {
                        return info.si_status;
                    } else {
                        // as in bash, add 128 to the signal number
                        return 128 + info.si_status;
                    }
                }
            }
        }
        }
    }

    int linux_namespaces_service_pipe[2];
    if (
        socketpair(AF_UNIX, SOCK_SEQPACKET, 0, linux_namespaces_service_pipe) ==
        -1
    ) {
        linux_namespaces_service_pipe[0] = -1;
        linux_namespaces_service_pipe[1] = -1;
        perror("<4>Failed to start Linux namespaces subsystem");
    }

    if (linux_namespaces_service_pipe[0] != -1) {
        shutdown(linux_namespaces_service_pipe[0], SHUT_WR);
        shutdown(linux_namespaces_service_pipe[1], SHUT_RD);

        switch (fork()) {
        case -1: {
            perror("<4>Failed to start Linux namespaces subsystem");
            close(linux_namespaces_service_pipe[0]);
            close(linux_namespaces_service_pipe[1]);
            linux_namespaces_service_pipe[0] = -1;
            linux_namespaces_service_pipe[1] = -1;
            break;
        }
        case 0:
            close(linux_namespaces_service_pipe[1]);
            explicit_bzero(buffer.data(), buffer.size());
            buffer.clear();
            buffer.shrink_to_fit();
            return emilua::app_context::linux_namespaces_service_main(
                linux_namespaces_service_pipe[0]);
        default: {
            close(linux_namespaces_service_pipe[0]);
            linux_namespaces_service_pipe[0] = -1;

            emilua::bzero_region args;
            args.s = nullptr;
            args.n = 0;
            write(linux_namespaces_service_pipe[1], &args, sizeof(args));
        }
        }
    }

    int main_ctx_concurrency_hint;
    fs::path entry_point;

    std::unordered_map<std::string, std::string> tmp_env;
    app_context appctx;
    appctx.app_args.reserve(2);
    appctx.app_args.emplace_back();
    appctx.app_args.emplace_back(entry_point.string());
    appctx.linux_namespaces_service_sockfd = linux_namespaces_service_pipe[1];

    {
        constexpr unsigned int aflags = boost::archive::no_header |
            boost::archive::no_codecvt |
            boost::archive::no_xml_tag_checking;
        std::istringstream is{buffer};
        boost::archive::binary_iarchive ia{is, aflags};

        std::string str;

        ia >> main_ctx_concurrency_hint;

        ia >> str;
        entry_point = fs::path{str, fs::path::native_format};
        str.clear();

        ia >> tmp_env;
        for (const auto &[k, v] : tmp_env) {
            setenv(k.c_str(), v.c_str(), /*overwrite=*/1);
            appctx.app_env.emplace(k, v);
        }
    }
    buffer.clear();
    buffer.shrink_to_fit();

    try {
        std::locale native_locale{""};
        std::locale::global(native_locale);
        std::cin.imbue(native_locale);
        std::cout.imbue(native_locale);
        std::cerr.imbue(native_locale);
        std::clog.imbue(native_locale);
    } catch (const std::exception& e) {
        fmt::print(
            boost::nowide::cerr,
            FMT_STRING("<4>Failed to set the native locale: `{}`\n"),
            e.what());
    }

    emilua::stdout_has_color = [&tmp_env]() {
        if (auto it = tmp_env.find("EMILUA_COLORS") ; it != tmp_env.end()) {
            if (it->second == "1") {
                return true;
            } else if (it->second == "0") {
                return false;
            } else if (it->second.size() > 0) {
                std::cerr <<
                    "<4>Ignoring unrecognized value for EMILUA_COLORS\n";
            }
        }

        if (proc_stderr == STDERR_FILENO)
            return static_cast<bool>(proc_stderr_has_color);

        return false;
    }();

    if (auto it = tmp_env.find("EMILUA_LOG_LEVELS") ; it != tmp_env.end()) {
        std::string_view env = it->second;
        int level;
        auto res = std::from_chars(
            env.data(), env.data() + env.size(), level);
        if (res.ec == std::errc{})
            emilua::log_domain<emilua::default_log_domain>::log_level = level;
    }

    asio::io_context ioctx{main_ctx_concurrency_hint};
    asio::make_service<properties_service>(ioctx, main_ctx_concurrency_hint);

    if (
        auto it = appctx.app_env.find("EMILUA_PATH") ;
        it != appctx.app_env.end()
    ) {
        for (std::string_view spec{it->second} ;;) {
            std::string_view::size_type sepidx = spec.find(':');
            if (sepidx == std::string_view::npos) {
                appctx.emilua_path.emplace_back(spec, fs::path::native_format);
                break;
            } else {
                appctx.emilua_path.emplace_back(
                    spec.substr(0, sepidx), fs::path::native_format);
                spec.remove_prefix(sepidx + 1);
            }
        }
    }

    try {
        auto vm_ctx = make_vm(ioctx, appctx, entry_point, ContextType::worker);
        appctx.master_vm = vm_ctx;

        ++vm_ctx->inbox.nsenders;
        auto inbox_service = new linux_container_inbox_service{
            ioctx, inboxfd};
        vm_ctx->pending_operations.push_back(*inbox_service);

        vm_ctx->strand().post([vm_ctx]() {
            vm_ctx->fiber_resume(
                vm_ctx->L(),
                hana::make_set(vm_context::options::skip_clear_interrupter));
        }, std::allocator<void>{});
    } catch (const std::exception& e) {
        std::cerr << "Error starting the lua VM: " << e.what() << std::endl;
        return 1;
    }

    ioctx.run();

    {
        std::unique_lock<std::mutex> lk{appctx.extra_threads_count_mtx};
        while (appctx.extra_threads_count > 0)
            appctx.extra_threads_count_empty_cond.wait(lk);
    }

    // for some reason the C runtime won't flush `stdout` on clone()d processes
    std::cout << std::flush;

    return appctx.exit_code;
}

int app_context::linux_namespaces_service_main(int sockfd)
{
    for (;;) {
        bzero_region args;
        auto nread = read(sockfd, &args, sizeof(args));
        if (nread == -1 || nread == 0)
            return 1;

        assert(nread == sizeof(args));
        if (args.s == nullptr)
            break;

        explicit_bzero(args.s, args.n);
    }
    clearenv();

    if (dup2(sockfd, 3) == -1) {
        perror("<3>linux_namespaces/supervisor");
        return 1;
    }
    sockfd = 3;

    if (close_range(4, UINT_MAX, /*flags=*/0) == -1) {
        perror("<3>linux_namespaces/supervisor");
        return 1;
    }

    {
        struct sigaction sa;
        sa.sa_handler = SIG_DFL;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_NOCLDWAIT;
        sigaction(SIGCHLD, /*act=*/&sa, /*oldact=*/NULL);

        // The only real concern is to ensure this process image
        // (/proc/self/exe) will stay ETXTBSY for as long as we have any
        // (untrusted) children.
        sigset_t set;
        sigfillset(&set);
        sigdelset(&set, SIGCHLD);
        sigprocmask(SIG_BLOCK, &set, /*oldset=*/NULL);
    }

    struct msghdr msg;

    // That's the only acceptable data in the supervisor process memory (file
    // descriptor numbers/actions). The supervisor process memory is cloned to
    // subsequent children and we don't want sensitive data leaking into
    // them. To avoid even loading any sensitive data in the supervisor process
    // memory we serialize and send directly to the children processes instead.
    linux_container_start_vm_request request;
    struct iovec iov;

    union {
        struct cmsghdr align;
        char buf[CMSG_SPACE(sizeof(int) * 4)];
    } cmsgu;

    // WARNING: Any exit-path now must wait for all children to finish!
    for (;;) {
        std::memset(&msg, 0, sizeof(msg));
        iov.iov_base = &request;
        iov.iov_len = sizeof(request);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = cmsgu.buf;
        msg.msg_controllen = sizeof(cmsgu.buf);

        auto nread = recvmsg(sockfd, &msg, /*flags=*/0);
        switch (nread) {
        case -1:
            perror("<3>linux_namespaces/supervisor");
            close(sockfd);
            while (wait(NULL) > 0);
            return 1;
        case 0:
            close(sockfd);
            while (wait(NULL) > 0);
            return 0;
        }
        assert(nread == sizeof(request));

        int fds[4] = {-1, -1, -1, -1};
        for (struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg) ; cmsg != NULL ;
             cmsg = CMSG_NXTHDR(&msg, cmsg)) {
            if (cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS)
                continue;

            assert(sizeof(fds) >= cmsg->cmsg_len - CMSG_LEN(0));
            std::memcpy(fds, CMSG_DATA(cmsg), cmsg->cmsg_len - CMSG_LEN(0));
            break;
        }
        if (msg.msg_flags & MSG_CTRUNC) {
            for (int i = 0 ; i != 4 ; ++i) {
                if (fds[i] == -1)
                    break;

                close(fds[i]);
            }
            continue;
        }
        inboxfd = fds[0];
        assert(inboxfd != -1);

        switch (request.stdin_action) {
        case linux_container_start_vm_request::CLOSE_FD:
            proc_stdin = -1;
            break;
        case linux_container_start_vm_request::SHARE_PARENT:
            proc_stdin = STDIN_FILENO;
            break;
        case linux_container_start_vm_request::USE_PIPE:
            for (int i = 1 ; i != 4 ; ++i) {
                if (fds[i] != -1) {
                    proc_stdin = fds[i];
                    fds[i] = -1;
                    break;
                }
            }
            assert(proc_stdin != -1);
            break;
        }

        switch (request.stdout_action) {
        case linux_container_start_vm_request::CLOSE_FD:
            proc_stdout = -1;
            break;
        case linux_container_start_vm_request::SHARE_PARENT:
            proc_stdout = STDOUT_FILENO;
            break;
        case linux_container_start_vm_request::USE_PIPE:
            for (int i = 1 ; i != 4 ; ++i) {
                if (fds[i] != -1) {
                    proc_stdout = fds[i];
                    fds[i] = -1;
                    break;
                }
            }
            assert(proc_stdout != -1);
            break;
        }

        switch (request.stderr_action) {
        case linux_container_start_vm_request::CLOSE_FD:
            proc_stderr = -1;
            break;
        case linux_container_start_vm_request::SHARE_PARENT:
            proc_stderr = STDERR_FILENO;
            proc_stderr_has_color = request.stderr_has_color;
            break;
        case linux_container_start_vm_request::USE_PIPE:
            for (int i = 1 ; i != 4 ; ++i) {
                if (fds[i] != -1) {
                    proc_stderr = fds[i];
                    fds[i] = -1;
                    break;
                }
            }
            assert(proc_stderr != -1);
            break;
        }
        assert(fds[1] == -1);
        assert(fds[2] == -1);
        assert(fds[3] == -1);

        has_lua_hook = request.has_lua_hook;

        linux_container_start_vm_reply reply;
        int pidfd = -1;
        request.clone_flags |= CLONE_PIDFD | SIGCHLD;
        reply.childpid = clone(child_main, clone_stack_address,
                               request.clone_flags, /*arg=*/nullptr, &pidfd);
        reply.error = (reply.childpid == -1) ? errno : 0;

        switch (proc_stdin) {
        case -1:
        case STDIN_FILENO:
            break;
        default:
            close(proc_stdin);
        }

        switch (proc_stdout) {
        case -1:
        case STDOUT_FILENO:
            break;
        default:
            close(proc_stdout);
        }

        switch (proc_stderr) {
        case -1:
        case STDERR_FILENO:
            break;
        default:
            close(proc_stderr);
        }

        std::memset(&msg, 0, sizeof(msg));
        iov.iov_base = &reply;
        iov.iov_len = sizeof(reply);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;

        if (pidfd != -1) {
            msg.msg_control = cmsgu.buf;
            msg.msg_controllen = CMSG_SPACE(sizeof(int));
            struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
            cmsg->cmsg_level = SOL_SOCKET;
            cmsg->cmsg_type = SCM_RIGHTS;
            cmsg->cmsg_len = CMSG_LEN(sizeof(int));
            std::memcpy(CMSG_DATA(cmsg), &pidfd, sizeof(int));
        }

        sendmsg(inboxfd, &msg, MSG_NOSIGNAL);
        close(inboxfd);
        if (pidfd != -1)
            close(pidfd);
    }
}

} // namespace emilua
