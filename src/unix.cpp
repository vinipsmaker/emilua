/* Copyright (c) 2022 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#include <boost/asio/local/connect_pair.hpp>
#include <boost/scope_exit.hpp>

#include <emilua/file_descriptor.hpp>
#include <emilua/dispatch_table.hpp>
#include <emilua/async_base.hpp>
#include <emilua/byte_span.hpp>
#include <emilua/unix.hpp>

// Linux internally defines this to 255
#define SCM_MAX_FD 255

namespace emilua {

char unix_key;
char unix_datagram_socket_mt_key;
char unix_stream_acceptor_mt_key;
char unix_stream_socket_mt_key;
char unix_seqpacket_acceptor_mt_key;
char unix_seqpacket_socket_mt_key;

static char unix_datagram_socket_receive_key;
static char unix_datagram_socket_receive_from_key;
static char unix_datagram_socket_send_key;
static char unix_datagram_socket_send_to_key;
static char unix_datagram_socket_receive_with_fds_key;
static char unix_datagram_socket_receive_from_with_fds_key;
static char unix_datagram_socket_send_with_fds_key;
static char unix_datagram_socket_send_to_with_fds_key;
static char unix_stream_acceptor_accept_key;
static char unix_stream_socket_connect_key;
static char unix_stream_socket_read_some_key;
static char unix_stream_socket_write_some_key;
static char unix_stream_socket_receive_with_fds_key;
static char unix_stream_socket_send_with_fds_key;
static char unix_seqpacket_acceptor_accept_key;
static char unix_seqpacket_socket_connect_key;
static char unix_seqpacket_socket_receive_key;
static char unix_seqpacket_socket_send_key;
static char unix_seqpacket_socket_receive_with_fds_key;
static char unix_seqpacket_socket_send_with_fds_key;

template<class T, bool SKIP_EOF_DETECTION, bool IS_RECEIVE_FROM>
struct receive_with_fds_op
    : public std::enable_shared_from_this<
        receive_with_fds_op<T, SKIP_EOF_DETECTION, IS_RECEIVE_FROM>>
{
    receive_with_fds_op(emilua::vm_context& vm_ctx,
                        asio::cancellation_slot cancel_slot,
                        T& sock,
                        const std::shared_ptr<unsigned char[]>& buffer,
                        lua_Integer buffer_size,
                        lua_Integer maxfds)
        : sock{sock}
        , current_fiber{vm_ctx.current_fiber()}
        , vm_ctx{vm_ctx.shared_from_this()}
        , cancel_slot{std::move(cancel_slot)}
        , buffer{buffer}
        , buffer_size{buffer_size}
        , maxfds{maxfds}
    {}

    void do_wait()
    {
        sock.socket.async_wait(
            asio::socket_base::wait_read,
            asio::bind_cancellation_slot(cancel_slot, asio::bind_executor(
                vm_ctx->strand_using_defer(),
                [self=this->shared_from_this()](
                    const boost::system::error_code& ec
                ) {
                    self->on_wait(ec);
                }
            ))
        );
    }

    void on_wait(const boost::system::error_code& ec)
    {
        if (!vm_ctx->valid())
            return;

        if (ec) {
            --sock.nbusy;
            vm_ctx->fiber_resume(
                current_fiber,
                hana::make_set(
                    vm_context::options::auto_detect_interrupt,
                    hana::make_pair(opt_args, hana::make_tuple(ec))));
            return;
        }

        struct msghdr msg;
        std::memset(&msg, 0, sizeof(msg));

        struct sockaddr_un remote_endpoint;
        if (IS_RECEIVE_FROM) {
            msg.msg_name = &remote_endpoint;
            msg.msg_namelen = sizeof(remote_endpoint);
        }

        struct iovec iov;
        iov.iov_base = buffer.get();
        iov.iov_len = buffer_size;
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;

        std::vector<struct cmsghdr> cmsgbuf;
        msg.msg_controllen = CMSG_SPACE(sizeof(int) * maxfds);
        cmsgbuf.resize(msg.msg_controllen / sizeof(struct cmsghdr) + 1);
        msg.msg_control = cmsgbuf.data();

        auto nread = recvmsg(sock.socket.native_handle(), &msg, MSG_DONTWAIT);
        if (nread == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            do_wait();
            return;
        }

        if (nread == -1) {
            --sock.nbusy;
            std::error_code ec2{errno, std::system_category()};
            vm_ctx->fiber_resume(
                current_fiber,
                hana::make_set(
                    vm_context::options::auto_detect_interrupt,
                    hana::make_pair(opt_args, hana::make_tuple(ec2))));
            return;
        }

        std::vector<int> fds;
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
            if (cmsg->cmsg_level != SOL_SOCKET ||
                cmsg->cmsg_type != SCM_RIGHTS) {
                continue;
            }

            int* in = (int*)CMSG_DATA(cmsg);
            auto nfds = (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int);
            for (std::size_t i = 0 ; i != nfds ; ++i) {
                int fd;
                std::memcpy(&fd, in++, sizeof(int));
                if (fd != -1)
                    fds.emplace_back(fd);
            }
        }

        // * SOCK_STREAM doesn't allow 0-byte reads. 0 is reserved to notify
        //   EOF.
        // * SOCK_DGRAM allows 0-byte datagrams. There's no EOF given the socket
        //   isn't even connection-oriented (although FreeBSD might report
        //   ECONNRESET on socketpair() sockets).
        // * SOCK_SEQPACKET allows 0-byte datagrams, but won't differentiate
        //   0-sized datagrams from EOF. Clearly there's a mismatch that
        //   prevented some synergy to emerge here.
        //
        // SOCK_SEQPACKET design is sloppy, some "colcha de retalhos" as
        // Brazilian folks would describe it. Given the design is flawed, we
        // have to take some liberties here. Emilua will ignore 0-sized messages
        // and just report EOF anyway (SOCK_SEQPACKET was supposed to represent
        // a connection-oriented socket and that's the trait that must be
        // preserved over unanticipated kludges).
        if (!SKIP_EOF_DETECTION && nread == 0) {
            --sock.nbusy;
            auto ec2 = make_error_code(asio::error::eof);
            vm_ctx->fiber_resume(
                current_fiber,
                hana::make_set(
                    vm_context::options::auto_detect_interrupt,
                    hana::make_pair(opt_args, hana::make_tuple(ec2))));
            return;
        }

        auto ep_pusher = [&msg,&remote_endpoint](lua_State* L) {
            if (msg.msg_namelen <= sizeof(sa_family_t)) {
                lua_pushnil(L);
                return;
            }

            if (remote_endpoint.sun_family != AF_UNIX) {
                lua_pushnil(L);
                return;
            }

            std::size_t len = msg.msg_namelen -
                offsetof(struct sockaddr_un, sun_path);
            assert(len > 0);
            if (remote_endpoint.sun_path[0] != '\0')
                --len;
            lua_pushlstring(L, remote_endpoint.sun_path, len);
        };

        auto fds_pusher = [&fds,maxfds=this->maxfds](lua_State* L) {
            lua_createtable(L, /*narr=*/fds.size(), /*nrec=*/0);
            rawgetp(L, LUA_REGISTRYINDEX, &file_descriptor_mt_key);

            int i = 0;
            for (auto& fd: fds) {
                auto fdhandle = static_cast<file_descriptor_handle*>(
                    lua_newuserdata(L, sizeof(file_descriptor_handle))
                );
                lua_pushvalue(L, -2);
                setmetatable(L, -2);
                *fdhandle = fd;
                fd = -1;
                lua_rawseti(L, -3, ++i);

                if (i == maxfds)
                    break;
            }

            lua_pop(L, 1);
        };

        --sock.nbusy;
        if (IS_RECEIVE_FROM) {
            vm_ctx->fiber_resume(
                current_fiber,
                hana::make_set(
                    vm_context::options::auto_detect_interrupt,
                    hana::make_pair(
                        opt_args,
                        hana::make_tuple(ec, nread, ep_pusher, fds_pusher))));
        } else {
            vm_ctx->fiber_resume(
                current_fiber,
                hana::make_set(
                    vm_context::options::auto_detect_interrupt,
                    hana::make_pair(
                        opt_args, hana::make_tuple(ec, nread, fds_pusher))));
        }
    }

    T& sock;
    lua_State* current_fiber;
    std::shared_ptr<emilua::vm_context> vm_ctx;
    asio::cancellation_slot cancel_slot;
    std::shared_ptr<unsigned char[]> buffer;
    lua_Integer buffer_size;
    lua_Integer maxfds;

    static constexpr auto opt_args = vm_context::options::arguments;
};

template<class T>
struct send_with_fds_op
    : public std::enable_shared_from_this<send_with_fds_op<T>>
{
    struct file_descriptor_lock
    {
        file_descriptor_lock(file_descriptor_handle* reference)
            : reference{reference}
            , value{*reference}
        {}

        file_descriptor_handle* reference;
        file_descriptor_handle value;
    };

    send_with_fds_op(emilua::vm_context& vm_ctx,
                     asio::cancellation_slot cancel_slot,
                     T& sock,
                     const std::shared_ptr<unsigned char[]>& buffer,
                     lua_Integer buffer_size)
        : sock{sock}
        , current_fiber{vm_ctx.current_fiber()}
        , vm_ctx{vm_ctx.shared_from_this()}
        , cancel_slot{std::move(cancel_slot)}
        , buffer{buffer}
        , buffer_size{buffer_size}
    {}

    void do_wait()
    {
        sock.socket.async_wait(
            asio::socket_base::wait_write,
            asio::bind_cancellation_slot(cancel_slot, asio::bind_executor(
                vm_ctx->strand_using_defer(),
                [self=this->shared_from_this()](
                    const boost::system::error_code& ec
                ) {
                    self->on_wait(ec);
                }
            ))
        );
    }

    void on_wait(const boost::system::error_code& ec)
    {
        if (!vm_ctx->valid()) {
            for (auto& fdlock: fds) {
                int res = close(fdlock.value);
                boost::ignore_unused(res);
            }
            return;
        }

        if (ec) {
            --sock.nbusy;
            for (auto& fdlock: fds) {
                *fdlock.reference = fdlock.value;
            }
            vm_ctx->fiber_resume(
                current_fiber,
                hana::make_set(
                    vm_context::options::auto_detect_interrupt,
                    hana::make_pair(opt_args, hana::make_tuple(ec))));
            return;
        }

        struct msghdr msg;
        std::memset(&msg, 0, sizeof(msg));

        asio::local::datagram_protocol::endpoint raw_ep{remote_endpoint};
        if (remote_endpoint.size() > 0) {
            msg.msg_name = raw_ep.data();
            msg.msg_namelen = raw_ep.size();
        }

        struct iovec iov;
        iov.iov_base = buffer.get();
        iov.iov_len = buffer_size;
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;

        std::vector<struct cmsghdr> cmsgbuf;
        msg.msg_controllen = CMSG_SPACE(sizeof(int) * fds.size());
        cmsgbuf.resize(msg.msg_controllen / sizeof(struct cmsghdr) + 1);
        msg.msg_control = cmsgbuf.data();

        struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        cmsg->cmsg_len = CMSG_LEN(sizeof(int) * fds.size());
        {
            int* out = (int*)CMSG_DATA(cmsg);
            for (auto& fdlock: fds) {
                std::memcpy(out++, &fdlock.value, sizeof(int));
            }
        }

        auto nwritten = sendmsg(sock.socket.native_handle(), &msg,
                                MSG_DONTWAIT | MSG_NOSIGNAL);
        if (nwritten == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            do_wait();
            return;
        }

        if (nwritten == -1) {
            --sock.nbusy;
            for (auto& fdlock: fds) {
                *fdlock.reference = fdlock.value;
            }
            std::error_code ec2{errno, std::system_category()};
            vm_ctx->fiber_resume(
                current_fiber,
                hana::make_set(
                    vm_context::options::auto_detect_interrupt,
                    hana::make_pair(opt_args, hana::make_tuple(ec2))));
            return;
        }

        --sock.nbusy;
        for (auto& fdlock: fds) {
            *fdlock.reference = fdlock.value;
        }
        vm_ctx->fiber_resume(
            current_fiber,
            hana::make_set(
                vm_context::options::auto_detect_interrupt,
                hana::make_pair(opt_args, hana::make_tuple(ec, nwritten))));
    }

    T& sock;
    lua_State* current_fiber;
    std::shared_ptr<emilua::vm_context> vm_ctx;
    asio::cancellation_slot cancel_slot;
    std::shared_ptr<unsigned char[]> buffer;
    lua_Integer buffer_size;
    std::vector<file_descriptor_lock> fds;
    std::string_view remote_endpoint;

    static constexpr auto opt_args = vm_context::options::arguments;
};

static int unix_datagram_socket_open(lua_State* L)
{
    auto sock = static_cast<unix_datagram_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_datagram_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    sock->socket.open(asio::local::datagram_protocol{}, ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int unix_datagram_socket_bind(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);
    auto sock = static_cast<unix_datagram_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_datagram_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    sock->socket.bind(tostringview(L, 2), ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int unix_datagram_socket_connect(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);
    auto sock = static_cast<unix_datagram_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_datagram_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    sock->socket.connect(tostringview(L, 2), ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int unix_datagram_socket_close(lua_State* L)
{
    auto sock = static_cast<unix_datagram_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_datagram_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    sock->socket.close(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int unix_datagram_socket_cancel(lua_State* L)
{
    auto sock = static_cast<unix_datagram_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_datagram_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    sock->socket.cancel(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int unix_datagram_socket_set_option(lua_State* L)
{
    lua_settop(L, 3);
    luaL_checktype(L, 2, LUA_TSTRING);

    auto socket = static_cast<unix_datagram_socket*>(lua_touserdata(L, 1));
    if (!socket || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_datagram_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("debug"),
                [&]() -> int {
                    luaL_checktype(L, 3, LUA_TBOOLEAN);
                    asio::socket_base::debug o(lua_toboolean(L, 3));
                    socket->socket.set_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("send_buffer_size"),
                [&]() -> int {
                    luaL_checktype(L, 3, LUA_TNUMBER);
                    asio::socket_base::send_buffer_size o(lua_tointeger(L, 3));
                    socket->socket.set_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("receive_buffer_size"),
                [&]() -> int {
                    luaL_checktype(L, 3, LUA_TNUMBER);
                    asio::socket_base::receive_buffer_size o(
                        lua_tointeger(L, 3));
                    socket->socket.set_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            )
        ),
        [L](std::string_view /*key*/) -> int {
            push(L, std::errc::not_supported);
            return lua_error(L);
        },
        tostringview(L, 2)
    );
    return 0;
}

static int unix_datagram_socket_get_option(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);

    auto socket = static_cast<unix_datagram_socket*>(lua_touserdata(L, 1));
    if (!socket || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_datagram_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("debug"),
                [&]() -> int {
                    asio::socket_base::debug o;
                    socket->socket.get_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    lua_pushboolean(L, o.value());
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("send_buffer_size"),
                [&]() -> int {
                    asio::socket_base::send_buffer_size o;
                    socket->socket.get_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    lua_pushinteger(L, o.value());
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("receive_buffer_size"),
                [&]() -> int {
                    asio::socket_base::receive_buffer_size o;
                    socket->socket.get_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    lua_pushinteger(L, o.value());
                    return 1;
                }
            )
        ),
        [L](std::string_view /*key*/) -> int {
            push(L, std::errc::not_supported);
            return lua_error(L);
        },
        tostringview(L, 2)
    );
}

static int unix_datagram_socket_receive(lua_State* L)
{
    lua_settop(L, 3);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto sock = static_cast<unix_datagram_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_datagram_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto bs = static_cast<byte_span_handle*>(lua_touserdata(L, 2));
    if (!bs || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &byte_span_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    // Lua BitOp underlying type is int32
    std::int32_t flags;
    switch (lua_type(L, 3)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    case LUA_TNIL:
        flags = 0;
        break;
    case LUA_TNUMBER:
        flags = lua_tointeger(L, 3);
    }

    auto cancel_slot = set_default_interrupter(L, *vm_ctx);

    ++sock->nbusy;
    sock->socket.async_receive(
        asio::buffer(bs->data.get(), bs->size),
        flags,
        asio::bind_cancellation_slot(cancel_slot, asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,buf=bs->data,sock](
                const boost::system::error_code& ec,
                std::size_t bytes_transferred
            ) {
                boost::ignore_unused(buf);
                if (!vm_ctx->valid())
                    return;

                --sock->nbusy;

                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(ec, bytes_transferred)))
                );
            }
        ))
    );

    return lua_yield(L, 0);
}

static int unix_datagram_socket_receive_from(lua_State* L)
{
    lua_settop(L, 3);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto sock = static_cast<unix_datagram_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_datagram_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto bs = static_cast<byte_span_handle*>(lua_touserdata(L, 2));
    if (!bs || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &byte_span_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    // Lua BitOp underlying type is int32
    std::int32_t flags;
    switch (lua_type(L, 3)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    case LUA_TNIL:
        flags = 0;
        break;
    case LUA_TNUMBER:
        flags = lua_tointeger(L, 3);
    }

    auto cancel_slot = set_default_interrupter(L, *vm_ctx);

    auto remote_sender =
        std::make_shared<asio::local::datagram_protocol::endpoint>();

    ++sock->nbusy;
    sock->socket.async_receive_from(
        asio::buffer(bs->data.get(), bs->size),
        *remote_sender,
        flags,
        asio::bind_cancellation_slot(cancel_slot, asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,remote_sender,buf=bs->data,sock](
                const boost::system::error_code& ec,
                std::size_t bytes_transferred
            ) {
                boost::ignore_unused(buf);
                if (!vm_ctx->valid())
                    return;

                --sock->nbusy;

                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(
                                ec, bytes_transferred, remote_sender->path())))
                );
            }
        ))
    );

    return lua_yield(L, 0);
}

static int unix_datagram_socket_send(lua_State* L)
{
    lua_settop(L, 3);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto sock = static_cast<unix_datagram_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_datagram_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto bs = static_cast<byte_span_handle*>(lua_touserdata(L, 2));
    if (!bs || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &byte_span_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    // Lua BitOp underlying type is int32
    std::int32_t flags;
    switch (lua_type(L, 3)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    case LUA_TNIL:
        flags = 0;
        break;
    case LUA_TNUMBER:
        flags = lua_tointeger(L, 3);
    }

    auto cancel_slot = set_default_interrupter(L, *vm_ctx);

    ++sock->nbusy;
    sock->socket.async_send(
        asio::buffer(bs->data.get(), bs->size),
        flags,
        asio::bind_cancellation_slot(cancel_slot, asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,buf=bs->data,sock](
                const boost::system::error_code& ec,
                std::size_t bytes_transferred
            ) {
                boost::ignore_unused(buf);
                if (!vm_ctx->valid())
                    return;

                --sock->nbusy;

                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(ec, bytes_transferred))));
            }
        ))
    );

    return lua_yield(L, 0);
}

static int unix_datagram_socket_send_to(lua_State* L)
{
    lua_settop(L, 4);
    luaL_checktype(L, 3, LUA_TSTRING);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto sock = static_cast<unix_datagram_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_datagram_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto bs = static_cast<byte_span_handle*>(lua_touserdata(L, 2));
    if (!bs || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &byte_span_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    // Lua BitOp underlying type is int32
    std::int32_t flags;
    switch (lua_type(L, 4)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 4);
        return lua_error(L);
    case LUA_TNIL:
        flags = 0;
        break;
    case LUA_TNUMBER:
        flags = lua_tointeger(L, 4);
    }

    auto cancel_slot = set_default_interrupter(L, *vm_ctx);

    ++sock->nbusy;
    sock->socket.async_send_to(
        asio::buffer(bs->data.get(), bs->size),
        asio::local::datagram_protocol::endpoint{tostringview(L, 3)},
        flags,
        asio::bind_cancellation_slot(cancel_slot, asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,buf=bs->data,sock](
                const boost::system::error_code& ec,
                std::size_t bytes_transferred
            ) {
                boost::ignore_unused(buf);
                if (!vm_ctx->valid())
                    return;

                --sock->nbusy;

                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(ec, bytes_transferred))));
            }
        ))
    );

    return lua_yield(L, 0);
}

static int unix_datagram_socket_receive_with_fds(lua_State* L)
{
    luaL_checktype(L, 3, LUA_TNUMBER);

    auto& vm_ctx = get_vm_context(L);
    EMILUA_CHECK_SUSPEND_ALLOWED(vm_ctx, L);

    auto sock = static_cast<unix_datagram_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_datagram_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto bs = static_cast<byte_span_handle*>(lua_touserdata(L, 2));
    if (!bs || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &byte_span_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    lua_Integer maxfds = lua_tointeger(L, 3);
    if (maxfds < 1) {
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    }

    // impose smaller limit now so overflow handling on the next layer of code
    // will be simpler (actually we won't even need to handle it as overflow
    // will be impossible)
    if (maxfds > SCM_MAX_FD)
        maxfds = SCM_MAX_FD;

    auto cancel_slot = set_default_interrupter(L, vm_ctx);

    ++sock->nbusy;
    auto op = std::make_shared<receive_with_fds_op<
        unix_datagram_socket, /*SKIP_EOF_DETECTION=*/true,
        /*IS_RECEIVE_FROM=*/false
    >>(vm_ctx, std::move(cancel_slot), *sock, bs->data, bs->size, maxfds);
    op->do_wait();

    return lua_yield(L, 0);
}

static int unix_datagram_socket_receive_from_with_fds(lua_State* L)
{
    luaL_checktype(L, 3, LUA_TNUMBER);

    auto& vm_ctx = get_vm_context(L);
    EMILUA_CHECK_SUSPEND_ALLOWED(vm_ctx, L);

    auto sock = static_cast<unix_datagram_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_datagram_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto bs = static_cast<byte_span_handle*>(lua_touserdata(L, 2));
    if (!bs || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &byte_span_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    lua_Integer maxfds = lua_tointeger(L, 3);
    if (maxfds < 1) {
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    }

    // impose smaller limit now so overflow handling on the next layer of code
    // will be simpler (actually we won't even need to handle it as overflow
    // will be impossible)
    if (maxfds > SCM_MAX_FD)
        maxfds = SCM_MAX_FD;

    auto cancel_slot = set_default_interrupter(L, vm_ctx);

    ++sock->nbusy;
    auto op = std::make_shared<receive_with_fds_op<
        unix_datagram_socket, /*SKIP_EOF_DETECTION=*/true,
        /*IS_RECEIVE_FROM=*/true
    >>(vm_ctx, std::move(cancel_slot), *sock, bs->data, bs->size, maxfds);
    op->do_wait();

    return lua_yield(L, 0);
}

static int unix_datagram_socket_send_with_fds(lua_State* L)
{
    luaL_checktype(L, 3, LUA_TTABLE);

    auto& vm_ctx = get_vm_context(L);
    EMILUA_CHECK_SUSPEND_ALLOWED(vm_ctx, L);

    auto sock = static_cast<unix_datagram_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_datagram_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto bs = static_cast<byte_span_handle*>(lua_touserdata(L, 2));
    if (!bs || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &byte_span_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    auto cancel_slot = set_default_interrupter(L, vm_ctx);

    auto op = std::make_shared<send_with_fds_op<unix_datagram_socket>>(
        vm_ctx, std::move(cancel_slot), *sock, bs->data, bs->size);

    rawgetp(L, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
    for (int i = 1 ;; ++i) {
        lua_rawgeti(L, 3, i);
        auto current_element_type = lua_type(L, -1);
        if (current_element_type == LUA_TNIL)
            break;

        switch (current_element_type) {
        default:
            assert(current_element_type != LUA_TNIL);
            push(L, std::errc::invalid_argument, "arg", 3);
            return lua_error(L);
        case LUA_TUSERDATA:
            break;
        }

        auto handle = static_cast<file_descriptor_handle*>(
            lua_touserdata(L, -1));
        if (!lua_getmetatable(L, -1)) {
            push(L, std::errc::invalid_argument, "arg", 3);
            return lua_error(L);
        }
        if (!lua_rawequal(L, -1, -3)) {
            push(L, std::errc::invalid_argument, "arg", 1);
            return lua_error(L);
        }
        if (*handle == INVALID_FILE_DESCRIPTOR) {
            push(L, std::errc::device_or_resource_busy);
            return lua_error(L);
        }

        bool found_in_the_set = false;
        for (auto& fdlock: op->fds) {
            if (fdlock.reference == handle) {
                found_in_the_set = true;
                break;
            }
        }

        if (!found_in_the_set)
            op->fds.emplace_back(handle);
        lua_pop(L, 2);
    }

    ++sock->nbusy;
    for (auto& fdlock: op->fds) {
        *fdlock.reference = INVALID_FILE_DESCRIPTOR;
    }
    op->do_wait();

    return lua_yield(L, 0);
}

static int unix_datagram_socket_send_to_with_fds(lua_State* L)
{
    luaL_checktype(L, 3, LUA_TSTRING);
    luaL_checktype(L, 4, LUA_TTABLE);

    auto& vm_ctx = get_vm_context(L);
    EMILUA_CHECK_SUSPEND_ALLOWED(vm_ctx, L);

    auto sock = static_cast<unix_datagram_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_datagram_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto bs = static_cast<byte_span_handle*>(lua_touserdata(L, 2));
    if (!bs || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &byte_span_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    auto cancel_slot = set_default_interrupter(L, vm_ctx);

    auto op = std::make_shared<send_with_fds_op<unix_datagram_socket>>(
        vm_ctx, std::move(cancel_slot), *sock, bs->data, bs->size);
    op->remote_endpoint = tostringview(L, 3);

    rawgetp(L, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
    for (int i = 1 ;; ++i) {
        lua_rawgeti(L, 4, i);
        auto current_element_type = lua_type(L, -1);
        if (current_element_type == LUA_TNIL)
            break;

        switch (current_element_type) {
        default:
            assert(current_element_type != LUA_TNIL);
            push(L, std::errc::invalid_argument, "arg", 4);
            return lua_error(L);
        case LUA_TUSERDATA:
            break;
        }

        auto handle = static_cast<file_descriptor_handle*>(
            lua_touserdata(L, -1));
        if (!lua_getmetatable(L, -1)) {
            push(L, std::errc::invalid_argument, "arg", 4);
            return lua_error(L);
        }
        if (!lua_rawequal(L, -1, -3)) {
            push(L, std::errc::invalid_argument, "arg", 1);
            return lua_error(L);
        }
        if (*handle == INVALID_FILE_DESCRIPTOR) {
            push(L, std::errc::device_or_resource_busy);
            return lua_error(L);
        }

        bool found_in_the_set = false;
        for (auto& fdlock: op->fds) {
            if (fdlock.reference == handle) {
                found_in_the_set = true;
                break;
            }
        }

        if (!found_in_the_set)
            op->fds.emplace_back(handle);
        lua_pop(L, 2);
    }

    ++sock->nbusy;
    for (auto& fdlock: op->fds) {
        *fdlock.reference = INVALID_FILE_DESCRIPTOR;
    }
    op->do_wait();

    return lua_yield(L, 0);
}

static int unix_datagram_socket_io_control(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);

    auto socket = static_cast<unix_datagram_socket*>(lua_touserdata(L, 1));
    if (!socket || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_datagram_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("bytes_readable"),
                [&]() -> int {
                    asio::socket_base::bytes_readable command;
                    socket->socket.io_control(command, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    lua_pushnumber(L, command.get());
                    return 1;
                }
            )
        ),
        [L](std::string_view /*key*/) -> int {
            push(L, std::errc::not_supported);
            return lua_error(L);
        },
        tostringview(L, 2)
    );
    return 0;
}

inline int unix_datagram_socket_is_open(lua_State* L)
{
    auto sock = static_cast<unix_datagram_socket*>(lua_touserdata(L, 1));
    lua_pushboolean(L, sock->socket.is_open());
    return 1;
}

inline int unix_datagram_socket_local_path(lua_State* L)
{
    auto sock = static_cast<unix_datagram_socket*>(lua_touserdata(L, 1));
    boost::system::error_code ec;
    auto ep = sock->socket.local_endpoint(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    push(L, ep.path());
    return 1;
}

inline int unix_datagram_socket_remote_path(lua_State* L)
{
    auto sock = static_cast<unix_datagram_socket*>(lua_touserdata(L, 1));
    boost::system::error_code ec;
    auto ep = sock->socket.remote_endpoint(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    push(L, ep.path());
    return 1;
}

static int unix_datagram_socket_mt_index(lua_State* L)
{
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("open"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_datagram_socket_open);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("bind"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_datagram_socket_bind);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("connect"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_datagram_socket_connect);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("close"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_datagram_socket_close);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("cancel"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_datagram_socket_cancel);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("set_option"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_datagram_socket_set_option);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("get_option"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_datagram_socket_get_option);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("receive"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX,
                            &unix_datagram_socket_receive_key);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("receive_from"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX,
                            &unix_datagram_socket_receive_from_key);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("send"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX,
                            &unix_datagram_socket_send_key);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("send_to"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX,
                            &unix_datagram_socket_send_to_key);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("receive_with_fds"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX,
                            &unix_datagram_socket_receive_with_fds_key);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("receive_from_with_fds"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX,
                            &unix_datagram_socket_receive_from_with_fds_key);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("send_with_fds"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX,
                            &unix_datagram_socket_send_with_fds_key);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("send_to_with_fds"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX,
                            &unix_datagram_socket_send_to_with_fds_key);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("io_control"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_datagram_socket_io_control);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("is_open"), unix_datagram_socket_is_open),
            hana::make_pair(
                BOOST_HANA_STRING("local_path"),
                unix_datagram_socket_local_path),
            hana::make_pair(
                BOOST_HANA_STRING("remote_path"),
                unix_datagram_socket_remote_path)
        ),
        [](std::string_view /*key*/, lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        },
        tostringview(L, 2),
        L
    );
}

static int unix_datagram_socket_new(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);
    auto sock = static_cast<unix_datagram_socket*>(
        lua_newuserdata(L, sizeof(unix_datagram_socket))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &unix_datagram_socket_mt_key);
    setmetatable(L, -2);
    new (sock) unix_datagram_socket{vm_ctx.strand().context()};
    return 1;
}

static int unix_datagram_socket_pair(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);

    auto sock1 = static_cast<unix_datagram_socket*>(
        lua_newuserdata(L, sizeof(unix_datagram_socket))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &unix_datagram_socket_mt_key);
    setmetatable(L, -2);
    new (sock1) unix_datagram_socket{vm_ctx.strand().context()};

    auto sock2 = static_cast<unix_datagram_socket*>(
        lua_newuserdata(L, sizeof(unix_datagram_socket))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &unix_datagram_socket_mt_key);
    setmetatable(L, -2);
    new (sock2) unix_datagram_socket{vm_ctx.strand().context()};

    boost::system::error_code ec;
    asio::local::connect_pair(sock1->socket, sock2->socket, ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 2;
}

static int unix_stream_socket_open(lua_State* L)
{
    auto sock = static_cast<unix_stream_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_stream_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    sock->socket.open(asio::local::stream_protocol{}, ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int unix_stream_socket_bind(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);
    auto sock = static_cast<unix_stream_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_stream_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    sock->socket.bind(tostringview(L, 2), ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int unix_stream_socket_close(lua_State* L)
{
    auto sock = static_cast<unix_stream_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_stream_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    sock->socket.close(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int unix_stream_socket_cancel(lua_State* L)
{
    auto sock = static_cast<unix_stream_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_stream_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    sock->socket.cancel(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int unix_stream_socket_io_control(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);

    auto socket = static_cast<unix_stream_socket*>(lua_touserdata(L, 1));
    if (!socket || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_stream_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("bytes_readable"),
                [&]() -> int {
                    asio::socket_base::bytes_readable command;
                    socket->socket.io_control(command, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    lua_pushnumber(L, command.get());
                    return 1;
                }
            )
        ),
        [L](std::string_view /*key*/) -> int {
            push(L, std::errc::not_supported);
            return lua_error(L);
        },
        tostringview(L, 2)
    );
    return 0;
}

static int unix_stream_socket_shutdown(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);

    auto socket = static_cast<unix_stream_socket*>(lua_touserdata(L, 1));
    if (!socket || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_stream_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    std::error_code ec;
    asio::local::stream_protocol::socket::shutdown_type what;
    dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("receive"),
                [&]() {
                    what =
                        asio::local::stream_protocol::socket::shutdown_receive;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("send"),
                [&]() {
                    what = asio::local::stream_protocol::socket::shutdown_send;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("both"),
                [&]() {
                    what = asio::local::stream_protocol::socket::shutdown_both;
                }
            )
        ),
        [&](std::string_view /*key*/) {
            ec = make_error_code(std::errc::invalid_argument);
        },
        tostringview(L, 2)
    );
    if (ec) {
        push(L, ec);
        return lua_error(L);
    }
    boost::system::error_code ec2;
    socket->socket.shutdown(what, ec2);
    if (ec2) {
        push(L, static_cast<std::error_code>(ec2));
        return lua_error(L);
    }
    return 0;
}

static int unix_stream_socket_connect(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);
    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto s = static_cast<unix_stream_socket*>(lua_touserdata(L, 1));
    if (!s || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_stream_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    asio::local::stream_protocol::endpoint ep{tostringview(L, 2)};

    auto cancel_slot = set_default_interrupter(L, *vm_ctx);

    ++s->nbusy;
    s->socket.async_connect(
        ep,
        asio::bind_cancellation_slot(cancel_slot, asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,s](const boost::system::error_code& ec) {
                if (!vm_ctx->valid())
                    return;

                --s->nbusy;

                auto opt_args = vm_context::options::arguments;
                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(opt_args, hana::make_tuple(ec))));
            }
        ))
    );

    return lua_yield(L, 0);
}

static int unix_stream_socket_read_some(lua_State* L)
{
    lua_settop(L, 2);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto s = static_cast<unix_stream_socket*>(lua_touserdata(L, 1));
    if (!s || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_stream_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto bs = static_cast<byte_span_handle*>(lua_touserdata(L, 2));
    if (!bs || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &byte_span_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    auto cancel_slot = set_default_interrupter(L, *vm_ctx);

    ++s->nbusy;
    s->socket.async_read_some(
        asio::buffer(bs->data.get(), bs->size),
        asio::bind_cancellation_slot(cancel_slot, asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,buf=bs->data,s](
                const boost::system::error_code& ec,
                std::size_t bytes_transferred
            ) {
                if (!vm_ctx->valid())
                    return;

                --s->nbusy;

                boost::ignore_unused(buf);
                auto opt_args = vm_context::options::arguments;
                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            opt_args,
                            hana::make_tuple(ec, bytes_transferred))));
            }
        ))
    );

    return lua_yield(L, 0);
}

static int unix_stream_socket_write_some(lua_State* L)
{
    lua_settop(L, 2);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto s = static_cast<unix_stream_socket*>(lua_touserdata(L, 1));
    if (!s || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_stream_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto bs = static_cast<byte_span_handle*>(lua_touserdata(L, 2));
    if (!bs || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &byte_span_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    auto cancel_slot = set_default_interrupter(L, *vm_ctx);

    ++s->nbusy;
    s->socket.async_write_some(
        asio::buffer(bs->data.get(), bs->size),
        asio::bind_cancellation_slot(cancel_slot, asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,buf=bs->data,s](
                const boost::system::error_code& ec,
                std::size_t bytes_transferred
            ) {
                if (!vm_ctx->valid())
                    return;

                --s->nbusy;

                boost::ignore_unused(buf);
                auto opt_args = vm_context::options::arguments;
                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            opt_args,
                            hana::make_tuple(ec, bytes_transferred))));
            }
        ))
    );

    return lua_yield(L, 0);
}

static int unix_stream_socket_receive_with_fds(lua_State* L)
{
    luaL_checktype(L, 3, LUA_TNUMBER);

    auto& vm_ctx = get_vm_context(L);
    EMILUA_CHECK_SUSPEND_ALLOWED(vm_ctx, L);

    auto s = static_cast<unix_stream_socket*>(lua_touserdata(L, 1));
    if (!s || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_stream_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto bs = static_cast<byte_span_handle*>(lua_touserdata(L, 2));
    if (!bs || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &byte_span_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    lua_Integer maxfds = lua_tointeger(L, 3);
    if (maxfds < 1) {
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    }

    // impose smaller limit now so overflow handling on the next layer of code
    // will be simpler (actually we won't even need to handle it as overflow
    // will be impossible)
    if (maxfds > SCM_MAX_FD)
        maxfds = SCM_MAX_FD;

    auto cancel_slot = set_default_interrupter(L, vm_ctx);

    ++s->nbusy;
    auto op = std::make_shared<receive_with_fds_op<
        unix_stream_socket, /*SKIP_EOF_DETECTION=*/false,
        /*IS_RECEIVE_FROM=*/false
    >>(vm_ctx, std::move(cancel_slot), *s, bs->data, bs->size, maxfds);
    op->do_wait();

    return lua_yield(L, 0);
}

static int unix_stream_socket_send_with_fds(lua_State* L)
{
    luaL_checktype(L, 3, LUA_TTABLE);

    auto& vm_ctx = get_vm_context(L);
    EMILUA_CHECK_SUSPEND_ALLOWED(vm_ctx, L);

    auto s = static_cast<unix_stream_socket*>(lua_touserdata(L, 1));
    if (!s || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_stream_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto bs = static_cast<byte_span_handle*>(lua_touserdata(L, 2));
    if (!bs || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &byte_span_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    auto cancel_slot = set_default_interrupter(L, vm_ctx);

    auto op = std::make_shared<send_with_fds_op<unix_stream_socket>>(
        vm_ctx, std::move(cancel_slot), *s, bs->data, bs->size);

    rawgetp(L, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
    for (int i = 1 ;; ++i) {
        lua_rawgeti(L, 3, i);
        auto current_element_type = lua_type(L, -1);
        if (current_element_type == LUA_TNIL)
            break;

        switch (current_element_type) {
        default:
            assert(current_element_type != LUA_TNIL);
            push(L, std::errc::invalid_argument, "arg", 3);
            return lua_error(L);
        case LUA_TUSERDATA:
            break;
        }

        auto handle = static_cast<file_descriptor_handle*>(
            lua_touserdata(L, -1));
        if (!lua_getmetatable(L, -1)) {
            push(L, std::errc::invalid_argument, "arg", 3);
            return lua_error(L);
        }
        if (!lua_rawequal(L, -1, -3)) {
            push(L, std::errc::invalid_argument, "arg", 1);
            return lua_error(L);
        }
        if (*handle == INVALID_FILE_DESCRIPTOR) {
            push(L, std::errc::device_or_resource_busy);
            return lua_error(L);
        }

        bool found_in_the_set = false;
        for (auto& fdlock: op->fds) {
            if (fdlock.reference == handle) {
                found_in_the_set = true;
                break;
            }
        }

        if (!found_in_the_set)
            op->fds.emplace_back(handle);
        lua_pop(L, 2);
    }

    ++s->nbusy;
    for (auto& fdlock: op->fds) {
        *fdlock.reference = INVALID_FILE_DESCRIPTOR;
    }
    op->do_wait();

    return lua_yield(L, 0);
}

static int unix_stream_socket_set_option(lua_State* L)
{
    lua_settop(L, 3);
    luaL_checktype(L, 2, LUA_TSTRING);

    auto socket = static_cast<unix_stream_socket*>(lua_touserdata(L, 1));
    if (!socket || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_stream_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("send_low_watermark"),
                [&]() -> int {
                    luaL_checktype(L, 3, LUA_TNUMBER);
                    asio::socket_base::send_low_watermark o(
                        lua_tointeger(L, 3));
                    socket->socket.set_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("send_buffer_size"),
                [&]() -> int {
                    luaL_checktype(L, 3, LUA_TNUMBER);
                    asio::socket_base::send_buffer_size o(lua_tointeger(L, 3));
                    socket->socket.set_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("receive_low_watermark"),
                [&]() -> int {
                    luaL_checktype(L, 3, LUA_TNUMBER);
                    asio::socket_base::receive_low_watermark o(
                        lua_tointeger(L, 3));
                    socket->socket.set_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("receive_buffer_size"),
                [&]() -> int {
                    luaL_checktype(L, 3, LUA_TNUMBER);
                    asio::socket_base::receive_buffer_size o(
                        lua_tointeger(L, 3));
                    socket->socket.set_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("debug"),
                [&]() -> int {
                    luaL_checktype(L, 3, LUA_TBOOLEAN);
                    asio::socket_base::debug o(lua_toboolean(L, 3));
                    socket->socket.set_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            )
        ),
        [L](std::string_view /*key*/) -> int {
            push(L, std::errc::not_supported);
            return lua_error(L);
        },
        tostringview(L, 2)
    );
}

static int unix_stream_socket_get_option(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);

    auto socket = static_cast<unix_stream_socket*>(lua_touserdata(L, 1));
    if (!socket || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_stream_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("send_low_watermark"),
                [&]() -> int {
                    asio::socket_base::send_low_watermark o;
                    socket->socket.get_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    lua_pushinteger(L, o.value());
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("send_buffer_size"),
                [&]() -> int {
                    asio::socket_base::send_buffer_size o;
                    socket->socket.get_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    lua_pushinteger(L, o.value());
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("receive_low_watermark"),
                [&]() -> int {
                    asio::socket_base::receive_low_watermark o;
                    socket->socket.get_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    lua_pushinteger(L, o.value());
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("receive_buffer_size"),
                [&]() -> int {
                    asio::socket_base::receive_buffer_size o;
                    socket->socket.get_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    lua_pushinteger(L, o.value());
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("debug"),
                [&]() -> int {
                    asio::socket_base::debug o;
                    socket->socket.get_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    lua_pushboolean(L, o.value());
                    return 1;
                }
            )
        ),
        [L](std::string_view /*key*/) -> int {
            push(L, std::errc::not_supported);
            return lua_error(L);
        },
        tostringview(L, 2)
    );
}

inline int unix_stream_socket_is_open(lua_State* L)
{
    auto sock = static_cast<unix_stream_socket*>(lua_touserdata(L, 1));
    lua_pushboolean(L, sock->socket.is_open());
    return 1;
}

inline int unix_stream_socket_local_path(lua_State* L)
{
    auto sock = static_cast<unix_stream_socket*>(lua_touserdata(L, 1));
    boost::system::error_code ec;
    auto ep = sock->socket.local_endpoint(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    push(L, ep.path());
    return 1;
}

inline int unix_stream_socket_remote_path(lua_State* L)
{
    auto sock = static_cast<unix_stream_socket*>(lua_touserdata(L, 1));
    boost::system::error_code ec;
    auto ep = sock->socket.remote_endpoint(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    push(L, ep.path());
    return 1;
}

static int unix_stream_socket_mt_index(lua_State* L)
{
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("open"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_stream_socket_open);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("bind"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_stream_socket_bind);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("close"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_stream_socket_close);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("cancel"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_stream_socket_cancel);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("io_control"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_stream_socket_io_control);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("shutdown"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_stream_socket_shutdown);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("connect"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX,
                            &unix_stream_socket_connect_key);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("read_some"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX,
                            &unix_stream_socket_read_some_key);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("write_some"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX,
                            &unix_stream_socket_write_some_key);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("receive_with_fds"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX,
                            &unix_stream_socket_receive_with_fds_key);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("send_with_fds"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX,
                            &unix_stream_socket_send_with_fds_key);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("set_option"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_stream_socket_set_option);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("get_option"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_stream_socket_get_option);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("is_open"), unix_stream_socket_is_open),
            hana::make_pair(
                BOOST_HANA_STRING("local_path"), unix_stream_socket_local_path),
            hana::make_pair(
                BOOST_HANA_STRING("remote_path"),
                unix_stream_socket_remote_path)
        ),
        [](std::string_view /*key*/, lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        },
        tostringview(L, 2),
        L
    );
}

static int unix_stream_socket_new(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);
    auto sock = static_cast<unix_stream_socket*>(
        lua_newuserdata(L, sizeof(unix_stream_socket))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &unix_stream_socket_mt_key);
    setmetatable(L, -2);
    new (sock) unix_stream_socket{vm_ctx.strand().context()};
    return 1;
}

static int unix_stream_socket_pair(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);

    auto sock1 = static_cast<unix_stream_socket*>(
        lua_newuserdata(L, sizeof(unix_stream_socket))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &unix_stream_socket_mt_key);
    setmetatable(L, -2);
    new (sock1) unix_stream_socket{vm_ctx.strand().context()};

    auto sock2 = static_cast<unix_stream_socket*>(
        lua_newuserdata(L, sizeof(unix_stream_socket))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &unix_stream_socket_mt_key);
    setmetatable(L, -2);
    new (sock2) unix_stream_socket{vm_ctx.strand().context()};

    boost::system::error_code ec;
    asio::local::connect_pair(sock1->socket, sock2->socket, ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 2;
}

static int unix_stream_acceptor_open(lua_State* L)
{
    auto acceptor = static_cast<asio::local::stream_protocol::acceptor*>(
        lua_touserdata(L, 1));
    if (!acceptor || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_stream_acceptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    acceptor->open(asio::local::stream_protocol{}, ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int unix_stream_acceptor_bind(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);
    auto acceptor = static_cast<asio::local::stream_protocol::acceptor*>(
        lua_touserdata(L, 1));
    if (!acceptor || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_stream_acceptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    acceptor->bind(tostringview(L, 2), ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int unix_stream_acceptor_listen(lua_State* L)
{
    lua_settop(L, 2);
    auto acceptor = static_cast<asio::local::stream_protocol::acceptor*>(
        lua_touserdata(L, 1));
    if (!acceptor || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_stream_acceptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    switch (lua_type(L, 2)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    case LUA_TNIL: {
        boost::system::error_code ec;
        acceptor->listen(asio::socket_base::max_listen_connections, ec);
        if (ec) {
            push(L, static_cast<std::error_code>(ec));
            return lua_error(L);
        }
        return 0;
    }
    case LUA_TNUMBER: {
        boost::system::error_code ec;
        acceptor->listen(lua_tointeger(L, 2), ec);
        if (ec) {
            push(L, static_cast<std::error_code>(ec));
            return lua_error(L);
        }
        return 0;
    }
    }
}

static int unix_stream_acceptor_accept(lua_State* L)
{
    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto acceptor = static_cast<asio::local::stream_protocol::acceptor*>(
        lua_touserdata(L, 1));
    if (!acceptor || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_stream_acceptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto cancel_slot = set_default_interrupter(L, *vm_ctx);

    acceptor->async_accept(
        asio::bind_cancellation_slot(cancel_slot, asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber](const boost::system::error_code& ec,
                                   asio::local::stream_protocol::socket peer) {
                auto peer_pusher = [&ec,&peer](lua_State* fiber) {
                    if (ec) {
                        lua_pushnil(fiber);
                    } else {
                        auto s = static_cast<unix_stream_socket*>(
                            lua_newuserdata(fiber, sizeof(unix_stream_socket)));
                        rawgetp(fiber, LUA_REGISTRYINDEX,
                                &unix_stream_socket_mt_key);
                        setmetatable(fiber, -2);
                        new (s) unix_stream_socket{std::move(peer)};
                    }
                };

                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(ec, peer_pusher))));
            }
        ))
    );

    return lua_yield(L, 0);
}

static int unix_stream_acceptor_close(lua_State* L)
{
    auto acceptor = static_cast<asio::local::stream_protocol::acceptor*>(
        lua_touserdata(L, 1));
    if (!acceptor || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_stream_acceptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    acceptor->close(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int unix_stream_acceptor_cancel(lua_State* L)
{
    auto acceptor = static_cast<asio::local::stream_protocol::acceptor*>(
        lua_touserdata(L, 1));
    if (!acceptor || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_stream_acceptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    acceptor->cancel(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int unix_stream_acceptor_set_option(lua_State* L)
{
    auto acceptor = static_cast<asio::local::stream_protocol::acceptor*>(
        lua_touserdata(L, 1));
    if (!acceptor || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_stream_acceptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("debug"),
                [&]() -> int {
                    luaL_checktype(L, 3, LUA_TBOOLEAN);
                    asio::socket_base::debug o(lua_toboolean(L, 3));
                    acceptor->set_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("enable_connection_aborted"),
                [&]() -> int {
                    luaL_checktype(L, 3, LUA_TBOOLEAN);
                    asio::socket_base::enable_connection_aborted o(
                        lua_toboolean(L, 3));
                    acceptor->set_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            )
        ),
        [L](std::string_view /*key*/) -> int {
            push(L, std::errc::not_supported);
            return lua_error(L);
        },
        tostringview(L, 2)
    );
    return 0;
}

static int unix_stream_acceptor_get_option(lua_State* L)
{
    auto acceptor = static_cast<asio::local::stream_protocol::acceptor*>(
        lua_touserdata(L, 1));
    if (!acceptor || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_stream_acceptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("debug"),
                [&]() -> int {
                    asio::socket_base::debug o;
                    acceptor->get_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    lua_pushboolean(L, o.value());
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("enable_connection_aborted"),
                [&]() -> int {
                    asio::socket_base::enable_connection_aborted o;
                    acceptor->get_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    lua_pushboolean(L, o.value());
                    return 1;
                }
            )
        ),
        [L](std::string_view /*key*/) -> int {
            push(L, std::errc::not_supported);
            return lua_error(L);
        },
        tostringview(L, 2)
    );
}

inline int unix_stream_acceptor_is_open(lua_State* L)
{
    auto acceptor = static_cast<asio::local::stream_protocol::acceptor*>(
        lua_touserdata(L, 1));
    lua_pushboolean(L, acceptor->is_open());
    return 1;
}

inline int unix_stream_acceptor_local_path(lua_State* L)
{
    auto acceptor = static_cast<asio::local::stream_protocol::acceptor*>(
        lua_touserdata(L, 1));
    boost::system::error_code ec;
    auto ep = acceptor->local_endpoint(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    push(L, ep.path());
    return 1;
}

static int unix_stream_acceptor_mt_index(lua_State* L)
{
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("open"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_stream_acceptor_open);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("bind"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_stream_acceptor_bind);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("listen"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_stream_acceptor_listen);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("accept"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX,
                            &unix_stream_acceptor_accept_key);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("close"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_stream_acceptor_close);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("cancel"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_stream_acceptor_cancel);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("set_option"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_stream_acceptor_set_option);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("get_option"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_stream_acceptor_get_option);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("is_open"), unix_stream_acceptor_is_open),
            hana::make_pair(
                BOOST_HANA_STRING("local_path"),
                unix_stream_acceptor_local_path)
        ),
        [](std::string_view /*key*/, lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        },
        tostringview(L, 2),
        L
    );
}

static int unix_stream_acceptor_new(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);
    auto s = static_cast<asio::local::stream_protocol::acceptor*>(
        lua_newuserdata(L, sizeof(asio::local::stream_protocol::acceptor))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &unix_stream_acceptor_mt_key);
    setmetatable(L, -2);
    new (s) asio::local::stream_protocol::acceptor{vm_ctx.strand().context()};
    return 1;
}

static int unix_seqpacket_socket_open(lua_State* L)
{
    auto sock = static_cast<unix_seqpacket_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_seqpacket_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    sock->socket.open(seqpacket_protocol{}, ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int unix_seqpacket_socket_bind(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);
    auto sock = static_cast<unix_seqpacket_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_seqpacket_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    sock->socket.bind(tostringview(L, 2), ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int unix_seqpacket_socket_close(lua_State* L)
{
    auto sock = static_cast<unix_seqpacket_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_seqpacket_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    sock->socket.close(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int unix_seqpacket_socket_cancel(lua_State* L)
{
    auto sock = static_cast<unix_seqpacket_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_seqpacket_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    sock->socket.cancel(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int unix_seqpacket_socket_shutdown(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);

    auto socket = static_cast<unix_seqpacket_socket*>(lua_touserdata(L, 1));
    if (!socket || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_seqpacket_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    std::error_code ec;
    seqpacket_protocol::socket::shutdown_type what;
    dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("receive"),
                [&]() {
                    what = seqpacket_protocol::socket::shutdown_receive;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("send"),
                [&]() {
                    what = seqpacket_protocol::socket::shutdown_send;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("both"),
                [&]() {
                    what = seqpacket_protocol::socket::shutdown_both;
                }
            )
        ),
        [&](std::string_view /*key*/) {
            ec = make_error_code(std::errc::invalid_argument);
        },
        tostringview(L, 2)
    );
    if (ec) {
        push(L, ec);
        return lua_error(L);
    }
    boost::system::error_code ec2;
    socket->socket.shutdown(what, ec2);
    if (ec2) {
        push(L, static_cast<std::error_code>(ec2));
        return lua_error(L);
    }
    return 0;
}

static int unix_seqpacket_socket_connect(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);
    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto s = static_cast<unix_seqpacket_socket*>(lua_touserdata(L, 1));
    if (!s || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_seqpacket_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    seqpacket_protocol::endpoint ep{tostringview(L, 2)};

    auto cancel_slot = set_default_interrupter(L, *vm_ctx);

    ++s->nbusy;
    s->socket.async_connect(
        ep,
        asio::bind_cancellation_slot(cancel_slot, asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,s](const boost::system::error_code& ec) {
                if (!vm_ctx->valid())
                    return;

                --s->nbusy;

                auto opt_args = vm_context::options::arguments;
                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(opt_args, hana::make_tuple(ec))));
            }
        ))
    );

    return lua_yield(L, 0);
}

static int unix_seqpacket_socket_receive(lua_State* L)
{
    lua_settop(L, 3);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto sock = static_cast<unix_seqpacket_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_seqpacket_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto bs = static_cast<byte_span_handle*>(lua_touserdata(L, 2));
    if (!bs || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &byte_span_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    // Lua BitOp underlying type is int32
    std::int32_t flags;
    switch (lua_type(L, 3)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    case LUA_TNIL:
        flags = 0;
        break;
    case LUA_TNUMBER:
        flags = lua_tointeger(L, 3);
    }

    auto cancel_slot = set_default_interrupter(L, *vm_ctx);

    ++sock->nbusy;
    sock->socket.async_receive(
        asio::buffer(bs->data.get(), bs->size),
        flags,
        asio::bind_cancellation_slot(cancel_slot, asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,buf=bs->data,sock](
                boost::system::error_code ec,
                std::size_t bytes_transferred
            ) {
                boost::ignore_unused(buf);
                if (!vm_ctx->valid())
                    return;

                --sock->nbusy;

                if (!ec && bytes_transferred == 0)
                    ec = asio::error::eof;

                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(ec, bytes_transferred)))
                );
            }
        ))
    );

    return lua_yield(L, 0);
}

static int unix_seqpacket_socket_send(lua_State* L)
{
    lua_settop(L, 3);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto sock = static_cast<unix_seqpacket_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_seqpacket_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto bs = static_cast<byte_span_handle*>(lua_touserdata(L, 2));
    if (!bs || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &byte_span_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    // Lua BitOp underlying type is int32
    std::int32_t flags;
    switch (lua_type(L, 3)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    case LUA_TNIL:
        flags = 0;
        break;
    case LUA_TNUMBER:
        flags = lua_tointeger(L, 3);
    }

    auto cancel_slot = set_default_interrupter(L, *vm_ctx);

    ++sock->nbusy;
    sock->socket.async_send(
        asio::buffer(bs->data.get(), bs->size),
        flags,
        asio::bind_cancellation_slot(cancel_slot, asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,buf=bs->data,sock](
                const boost::system::error_code& ec,
                std::size_t bytes_transferred
            ) {
                boost::ignore_unused(buf);
                if (!vm_ctx->valid())
                    return;

                --sock->nbusy;

                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(ec, bytes_transferred))));
            }
        ))
    );

    return lua_yield(L, 0);
}

static int unix_seqpacket_socket_receive_with_fds(lua_State* L)
{
    luaL_checktype(L, 3, LUA_TNUMBER);

    auto& vm_ctx = get_vm_context(L);
    EMILUA_CHECK_SUSPEND_ALLOWED(vm_ctx, L);

    auto sock = static_cast<unix_seqpacket_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_seqpacket_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto bs = static_cast<byte_span_handle*>(lua_touserdata(L, 2));
    if (!bs || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &byte_span_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    lua_Integer maxfds = lua_tointeger(L, 3);
    if (maxfds < 1) {
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    }

    // impose smaller limit now so overflow handling on the next layer of code
    // will be simpler (actually we won't even need to handle it as overflow
    // will be impossible)
    if (maxfds > SCM_MAX_FD)
        maxfds = SCM_MAX_FD;

    auto cancel_slot = set_default_interrupter(L, vm_ctx);

    ++sock->nbusy;
    auto op = std::make_shared<receive_with_fds_op<
        unix_seqpacket_socket, /*SKIP_EOF_DETECTION=*/false,
        /*IS_RECEIVE_FROM=*/false
    >>(vm_ctx, std::move(cancel_slot), *sock, bs->data, bs->size, maxfds);
    op->do_wait();

    return lua_yield(L, 0);
}

static int unix_seqpacket_socket_send_with_fds(lua_State* L)
{
    luaL_checktype(L, 3, LUA_TTABLE);

    auto& vm_ctx = get_vm_context(L);
    EMILUA_CHECK_SUSPEND_ALLOWED(vm_ctx, L);

    auto sock = static_cast<unix_seqpacket_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_seqpacket_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto bs = static_cast<byte_span_handle*>(lua_touserdata(L, 2));
    if (!bs || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &byte_span_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    auto cancel_slot = set_default_interrupter(L, vm_ctx);

    auto op = std::make_shared<send_with_fds_op<unix_seqpacket_socket>>(
        vm_ctx, std::move(cancel_slot), *sock, bs->data, bs->size);

    rawgetp(L, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
    for (int i = 1 ;; ++i) {
        lua_rawgeti(L, 3, i);
        auto current_element_type = lua_type(L, -1);
        if (current_element_type == LUA_TNIL)
            break;

        switch (current_element_type) {
        default:
            assert(current_element_type != LUA_TNIL);
            push(L, std::errc::invalid_argument, "arg", 3);
            return lua_error(L);
        case LUA_TUSERDATA:
            break;
        }

        auto handle = static_cast<file_descriptor_handle*>(
            lua_touserdata(L, -1));
        if (!lua_getmetatable(L, -1)) {
            push(L, std::errc::invalid_argument, "arg", 3);
            return lua_error(L);
        }
        if (!lua_rawequal(L, -1, -3)) {
            push(L, std::errc::invalid_argument, "arg", 1);
            return lua_error(L);
        }
        if (*handle == INVALID_FILE_DESCRIPTOR) {
            push(L, std::errc::device_or_resource_busy);
            return lua_error(L);
        }

        bool found_in_the_set = false;
        for (auto& fdlock: op->fds) {
            if (fdlock.reference == handle) {
                found_in_the_set = true;
                break;
            }
        }

        if (!found_in_the_set)
            op->fds.emplace_back(handle);
        lua_pop(L, 2);
    }

    ++sock->nbusy;
    for (auto& fdlock: op->fds) {
        *fdlock.reference = INVALID_FILE_DESCRIPTOR;
    }
    op->do_wait();

    return lua_yield(L, 0);
}

static int unix_seqpacket_socket_set_option(lua_State* L)
{
    lua_settop(L, 3);
    luaL_checktype(L, 2, LUA_TSTRING);

    auto socket = static_cast<unix_seqpacket_socket*>(lua_touserdata(L, 1));
    if (!socket || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_seqpacket_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("send_buffer_size"),
                [&]() -> int {
                    luaL_checktype(L, 3, LUA_TNUMBER);
                    asio::socket_base::send_buffer_size o(lua_tointeger(L, 3));
                    socket->socket.set_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("receive_buffer_size"),
                [&]() -> int {
                    luaL_checktype(L, 3, LUA_TNUMBER);
                    asio::socket_base::receive_buffer_size o(
                        lua_tointeger(L, 3));
                    socket->socket.set_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("debug"),
                [&]() -> int {
                    luaL_checktype(L, 3, LUA_TBOOLEAN);
                    asio::socket_base::debug o(lua_toboolean(L, 3));
                    socket->socket.set_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            )
        ),
        [L](std::string_view /*key*/) -> int {
            push(L, std::errc::not_supported);
            return lua_error(L);
        },
        tostringview(L, 2)
    );
}

static int unix_seqpacket_socket_get_option(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);

    auto socket = static_cast<unix_seqpacket_socket*>(lua_touserdata(L, 1));
    if (!socket || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_seqpacket_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("send_buffer_size"),
                [&]() -> int {
                    asio::socket_base::send_buffer_size o;
                    socket->socket.get_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    lua_pushinteger(L, o.value());
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("receive_buffer_size"),
                [&]() -> int {
                    asio::socket_base::receive_buffer_size o;
                    socket->socket.get_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    lua_pushinteger(L, o.value());
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("debug"),
                [&]() -> int {
                    asio::socket_base::debug o;
                    socket->socket.get_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    lua_pushboolean(L, o.value());
                    return 1;
                }
            )
        ),
        [L](std::string_view /*key*/) -> int {
            push(L, std::errc::not_supported);
            return lua_error(L);
        },
        tostringview(L, 2)
    );
}

static int unix_seqpacket_socket_io_control(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);

    auto socket = static_cast<unix_seqpacket_socket*>(lua_touserdata(L, 1));
    if (!socket || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_seqpacket_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("bytes_readable"),
                [&]() -> int {
                    asio::socket_base::bytes_readable command;
                    socket->socket.io_control(command, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    lua_pushnumber(L, command.get());
                    return 1;
                }
            )
        ),
        [L](std::string_view /*key*/) -> int {
            push(L, std::errc::not_supported);
            return lua_error(L);
        },
        tostringview(L, 2)
    );
    return 0;
}

inline int unix_seqpacket_socket_is_open(lua_State* L)
{
    auto sock = static_cast<unix_seqpacket_socket*>(lua_touserdata(L, 1));
    lua_pushboolean(L, sock->socket.is_open());
    return 1;
}

inline int unix_seqpacket_socket_local_path(lua_State* L)
{
    auto sock = static_cast<unix_seqpacket_socket*>(lua_touserdata(L, 1));
    boost::system::error_code ec;
    auto ep = sock->socket.local_endpoint(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    push(L, ep.path());
    return 1;
}

inline int unix_seqpacket_socket_remote_path(lua_State* L)
{
    auto sock = static_cast<unix_seqpacket_socket*>(lua_touserdata(L, 1));
    boost::system::error_code ec;
    auto ep = sock->socket.remote_endpoint(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    push(L, ep.path());
    return 1;
}

static int unix_seqpacket_socket_mt_index(lua_State* L)
{
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("open"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_seqpacket_socket_open);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("bind"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_seqpacket_socket_bind);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("close"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_seqpacket_socket_close);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("cancel"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_seqpacket_socket_cancel);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("shutdown"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_seqpacket_socket_shutdown);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("connect"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX,
                            &unix_seqpacket_socket_connect_key);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("receive"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX,
                            &unix_seqpacket_socket_receive_key);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("send"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX,
                            &unix_seqpacket_socket_send_key);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("receive_with_fds"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX,
                            &unix_seqpacket_socket_receive_with_fds_key);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("send_with_fds"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX,
                            &unix_seqpacket_socket_send_with_fds_key);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("set_option"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_seqpacket_socket_set_option);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("get_option"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_seqpacket_socket_get_option);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("io_control"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_seqpacket_socket_io_control);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("is_open"), unix_seqpacket_socket_is_open),
            hana::make_pair(
                BOOST_HANA_STRING("local_path"),
                unix_seqpacket_socket_local_path),
            hana::make_pair(
                BOOST_HANA_STRING("remote_path"),
                unix_seqpacket_socket_remote_path)
        ),
        [](std::string_view /*key*/, lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        },
        tostringview(L, 2),
        L
    );
}

static int unix_seqpacket_socket_new(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);
    auto sock = static_cast<unix_seqpacket_socket*>(
        lua_newuserdata(L, sizeof(unix_seqpacket_socket))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &unix_seqpacket_socket_mt_key);
    setmetatable(L, -2);
    new (sock) unix_seqpacket_socket{vm_ctx.strand().context()};
    return 1;
}

static int unix_seqpacket_socket_pair(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);

    auto sock1 = static_cast<unix_seqpacket_socket*>(
        lua_newuserdata(L, sizeof(unix_seqpacket_socket))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &unix_seqpacket_socket_mt_key);
    setmetatable(L, -2);
    new (sock1) unix_seqpacket_socket{vm_ctx.strand().context()};

    auto sock2 = static_cast<unix_seqpacket_socket*>(
        lua_newuserdata(L, sizeof(unix_seqpacket_socket))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &unix_seqpacket_socket_mt_key);
    setmetatable(L, -2);
    new (sock2) unix_seqpacket_socket{vm_ctx.strand().context()};

    boost::system::error_code ec;
    asio::local::connect_pair(sock1->socket, sock2->socket, ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 2;
}

static int unix_seqpacket_acceptor_open(lua_State* L)
{
    auto acceptor = static_cast<seqpacket_protocol::acceptor*>(
        lua_touserdata(L, 1));
    if (!acceptor || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_seqpacket_acceptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    acceptor->open(seqpacket_protocol{}, ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int unix_seqpacket_acceptor_bind(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);
    auto acceptor = static_cast<seqpacket_protocol::acceptor*>(
        lua_touserdata(L, 1));
    if (!acceptor || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_seqpacket_acceptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    acceptor->bind(tostringview(L, 2), ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int unix_seqpacket_acceptor_listen(lua_State* L)
{
    lua_settop(L, 2);
    auto acceptor = static_cast<seqpacket_protocol::acceptor*>(
        lua_touserdata(L, 1));
    if (!acceptor || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_seqpacket_acceptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    switch (lua_type(L, 2)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    case LUA_TNIL: {
        boost::system::error_code ec;
        acceptor->listen(asio::socket_base::max_listen_connections, ec);
        if (ec) {
            push(L, static_cast<std::error_code>(ec));
            return lua_error(L);
        }
        return 0;
    }
    case LUA_TNUMBER: {
        boost::system::error_code ec;
        acceptor->listen(lua_tointeger(L, 2), ec);
        if (ec) {
            push(L, static_cast<std::error_code>(ec));
            return lua_error(L);
        }
        return 0;
    }
    }
}

static int unix_seqpacket_acceptor_accept(lua_State* L)
{
    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto acceptor = static_cast<seqpacket_protocol::acceptor*>(
        lua_touserdata(L, 1));
    if (!acceptor || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_seqpacket_acceptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto cancel_slot = set_default_interrupter(L, *vm_ctx);

    acceptor->async_accept(
        asio::bind_cancellation_slot(cancel_slot, asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber](const boost::system::error_code& ec,
                                   seqpacket_protocol::socket peer) {
                auto peer_pusher = [&ec,&peer](lua_State* fiber) {
                    if (ec) {
                        lua_pushnil(fiber);
                    } else {
                        auto s = static_cast<unix_seqpacket_socket*>(
                            lua_newuserdata(
                                fiber, sizeof(unix_seqpacket_socket)));
                        rawgetp(fiber, LUA_REGISTRYINDEX,
                                &unix_seqpacket_socket_mt_key);
                        setmetatable(fiber, -2);
                        new (s) unix_seqpacket_socket{std::move(peer)};
                    }
                };

                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(ec, peer_pusher))));
            }
        ))
    );

    return lua_yield(L, 0);
}

static int unix_seqpacket_acceptor_close(lua_State* L)
{
    auto acceptor = static_cast<seqpacket_protocol::acceptor*>(
        lua_touserdata(L, 1));
    if (!acceptor || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_seqpacket_acceptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    acceptor->close(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int unix_seqpacket_acceptor_cancel(lua_State* L)
{
    auto acceptor = static_cast<seqpacket_protocol::acceptor*>(
        lua_touserdata(L, 1));
    if (!acceptor || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_seqpacket_acceptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    acceptor->cancel(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int unix_seqpacket_acceptor_set_option(lua_State* L)
{
    auto acceptor = static_cast<seqpacket_protocol::acceptor*>(
        lua_touserdata(L, 1));
    if (!acceptor || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_seqpacket_acceptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("debug"),
                [&]() -> int {
                    luaL_checktype(L, 3, LUA_TBOOLEAN);
                    asio::socket_base::debug o(lua_toboolean(L, 3));
                    acceptor->set_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("enable_connection_aborted"),
                [&]() -> int {
                    luaL_checktype(L, 3, LUA_TBOOLEAN);
                    asio::socket_base::enable_connection_aborted o(
                        lua_toboolean(L, 3));
                    acceptor->set_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            )
        ),
        [L](std::string_view /*key*/) -> int {
            push(L, std::errc::not_supported);
            return lua_error(L);
        },
        tostringview(L, 2)
    );
    return 0;
}

static int unix_seqpacket_acceptor_get_option(lua_State* L)
{
    auto acceptor = static_cast<seqpacket_protocol::acceptor*>(
        lua_touserdata(L, 1));
    if (!acceptor || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_seqpacket_acceptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("debug"),
                [&]() -> int {
                    asio::socket_base::debug o;
                    acceptor->get_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    lua_pushboolean(L, o.value());
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("enable_connection_aborted"),
                [&]() -> int {
                    asio::socket_base::enable_connection_aborted o;
                    acceptor->get_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    lua_pushboolean(L, o.value());
                    return 1;
                }
            )
        ),
        [L](std::string_view /*key*/) -> int {
            push(L, std::errc::not_supported);
            return lua_error(L);
        },
        tostringview(L, 2)
    );
}

inline int unix_seqpacket_acceptor_is_open(lua_State* L)
{
    auto acceptor = static_cast<seqpacket_protocol::acceptor*>(
        lua_touserdata(L, 1));
    lua_pushboolean(L, acceptor->is_open());
    return 1;
}

inline int unix_seqpacket_acceptor_local_path(lua_State* L)
{
    auto acceptor = static_cast<seqpacket_protocol::acceptor*>(
        lua_touserdata(L, 1));
    boost::system::error_code ec;
    auto ep = acceptor->local_endpoint(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    push(L, ep.path());
    return 1;
}

static int unix_seqpacket_acceptor_mt_index(lua_State* L)
{
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("open"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_seqpacket_acceptor_open);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("bind"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_seqpacket_acceptor_bind);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("listen"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_seqpacket_acceptor_listen);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("accept"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX,
                            &unix_seqpacket_acceptor_accept_key);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("close"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_seqpacket_acceptor_close);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("cancel"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_seqpacket_acceptor_cancel);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("set_option"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_seqpacket_acceptor_set_option);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("get_option"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_seqpacket_acceptor_get_option);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("is_open"), unix_seqpacket_acceptor_is_open),
            hana::make_pair(
                BOOST_HANA_STRING("local_path"),
                unix_seqpacket_acceptor_local_path)
        ),
        [](std::string_view /*key*/, lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        },
        tostringview(L, 2),
        L
    );
}

static int unix_seqpacket_acceptor_new(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);
    auto a = static_cast<seqpacket_protocol::acceptor*>(
        lua_newuserdata(L, sizeof(seqpacket_protocol::acceptor))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &unix_seqpacket_acceptor_mt_key);
    setmetatable(L, -2);
    new (a) seqpacket_protocol::acceptor{vm_ctx.strand().context()};
    return 1;
}

void init_unix(lua_State* L)
{
    lua_pushlightuserdata(L, &unix_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/6);

        lua_pushliteral(L, "message_flag");
        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/1);

            lua_pushliteral(L, "peek");
            lua_pushinteger(L, asio::socket_base::message_peek);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);

        lua_pushliteral(L, "datagram_socket");
        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/1);

            lua_pushliteral(L, "new");
            lua_pushcfunction(L, unix_datagram_socket_new);
            lua_rawset(L, -3);

            lua_pushliteral(L, "pair");
            lua_pushcfunction(L, unix_datagram_socket_pair);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);

        lua_pushliteral(L, "stream_socket");
        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/2);

            lua_pushliteral(L, "new");
            lua_pushcfunction(L, unix_stream_socket_new);
            lua_rawset(L, -3);

            lua_pushliteral(L, "pair");
            lua_pushcfunction(L, unix_stream_socket_pair);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);

        lua_pushliteral(L, "stream_acceptor");
        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/1);

            lua_pushliteral(L, "new");
            lua_pushcfunction(L, unix_stream_acceptor_new);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);

        lua_pushliteral(L, "seqpacket_socket");
        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/2);

            lua_pushliteral(L, "new");
            lua_pushcfunction(L, unix_seqpacket_socket_new);
            lua_rawset(L, -3);

            lua_pushliteral(L, "pair");
            lua_pushcfunction(L, unix_seqpacket_socket_pair);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);

        lua_pushliteral(L, "seqpacket_acceptor");
        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/1);

            lua_pushliteral(L, "new");
            lua_pushcfunction(L, unix_seqpacket_acceptor_new);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &unix_datagram_socket_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "unix.datagram_socket");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, unix_datagram_socket_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<unix_datagram_socket>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &unix_stream_socket_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "unix.stream_socket");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, unix_stream_socket_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<unix_stream_socket>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &unix_stream_acceptor_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "unix.stream_acceptor");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, unix_stream_acceptor_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<asio::local::stream_protocol::acceptor>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &unix_seqpacket_socket_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "unix.seqpacket_socket");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, unix_seqpacket_socket_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<unix_seqpacket_socket>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &unix_seqpacket_acceptor_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "unix.seqpacket_acceptor");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, unix_seqpacket_acceptor_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<seqpacket_protocol::acceptor>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &unix_datagram_socket_receive_key);
    rawgetp(L, LUA_REGISTRYINDEX,
            &var_args__retval1_to_error__fwd_retval2__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, unix_datagram_socket_receive);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &unix_datagram_socket_receive_from_key);
    rawgetp(L, LUA_REGISTRYINDEX,
            &var_args__retval1_to_error__fwd_retval23__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, unix_datagram_socket_receive_from);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &unix_datagram_socket_send_key);
    rawgetp(L, LUA_REGISTRYINDEX,
            &var_args__retval1_to_error__fwd_retval2__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, unix_datagram_socket_send);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &unix_datagram_socket_send_to_key);
    rawgetp(L, LUA_REGISTRYINDEX,
            &var_args__retval1_to_error__fwd_retval2__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, unix_datagram_socket_send_to);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &unix_datagram_socket_receive_with_fds_key);
    rawgetp(L, LUA_REGISTRYINDEX,
            &var_args__retval1_to_error__fwd_retval23__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, unix_datagram_socket_receive_with_fds);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &unix_datagram_socket_receive_from_with_fds_key);
    rawgetp(L, LUA_REGISTRYINDEX,
            &var_args__retval1_to_error__fwd_retval234__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, unix_datagram_socket_receive_from_with_fds);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &unix_datagram_socket_send_with_fds_key);
    rawgetp(L, LUA_REGISTRYINDEX,
            &var_args__retval1_to_error__fwd_retval2__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, unix_datagram_socket_send_with_fds);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &unix_datagram_socket_send_to_with_fds_key);
    rawgetp(L, LUA_REGISTRYINDEX,
            &var_args__retval1_to_error__fwd_retval2__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, unix_datagram_socket_send_to_with_fds);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &unix_stream_socket_connect_key);
    rawgetp(L, LUA_REGISTRYINDEX, &var_args__retval1_to_error__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, unix_stream_socket_connect);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &unix_stream_socket_read_some_key);
    rawgetp(L, LUA_REGISTRYINDEX,
            &var_args__retval1_to_error__fwd_retval2__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, unix_stream_socket_read_some);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &unix_stream_socket_write_some_key);
    rawgetp(L, LUA_REGISTRYINDEX,
            &var_args__retval1_to_error__fwd_retval2__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, unix_stream_socket_write_some);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &unix_stream_socket_receive_with_fds_key);
    rawgetp(L, LUA_REGISTRYINDEX,
            &var_args__retval1_to_error__fwd_retval23__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, unix_stream_socket_receive_with_fds);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &unix_stream_socket_send_with_fds_key);
    rawgetp(L, LUA_REGISTRYINDEX,
            &var_args__retval1_to_error__fwd_retval2__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, unix_stream_socket_send_with_fds);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &unix_stream_acceptor_accept_key);
    rawgetp(L, LUA_REGISTRYINDEX,
            &var_args__retval1_to_error__fwd_retval2__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, unix_stream_acceptor_accept);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &unix_seqpacket_socket_connect_key);
    rawgetp(L, LUA_REGISTRYINDEX, &var_args__retval1_to_error__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, unix_seqpacket_socket_connect);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &unix_seqpacket_socket_receive_key);
    rawgetp(L, LUA_REGISTRYINDEX,
            &var_args__retval1_to_error__fwd_retval2__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, unix_seqpacket_socket_receive);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &unix_seqpacket_socket_send_key);
    rawgetp(L, LUA_REGISTRYINDEX,
            &var_args__retval1_to_error__fwd_retval2__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, unix_seqpacket_socket_send);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &unix_seqpacket_socket_receive_with_fds_key);
    rawgetp(L, LUA_REGISTRYINDEX,
            &var_args__retval1_to_error__fwd_retval23__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, unix_seqpacket_socket_receive_with_fds);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &unix_seqpacket_socket_send_with_fds_key);
    rawgetp(L, LUA_REGISTRYINDEX,
            &var_args__retval1_to_error__fwd_retval2__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, unix_seqpacket_socket_send_with_fds);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &unix_seqpacket_acceptor_accept_key);
    rawgetp(L, LUA_REGISTRYINDEX,
            &var_args__retval1_to_error__fwd_retval2__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, unix_seqpacket_acceptor_accept);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);
}

} // namespace emilua
