/* Copyright (c) 2021, 2022, 2023 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#include <emilua/system.hpp>
#include <emilua/async_base.hpp>
#include <emilua/byte_span.hpp>

#include <csignal>
#include <cstdlib>

#include <boost/preprocessor/control/iif.hpp>
#include <boost/predef/os/windows.h>
#include <boost/asio/signal_set.hpp>
#include <boost/vmd/is_number.hpp>
#include <boost/predef/os/macos.h>
#include <boost/scope_exit.hpp>
#include <boost/vmd/empty.hpp>

#include <emilua/file_descriptor.hpp>
#include <emilua/dispatch_table.hpp>

#if BOOST_OS_WINDOWS
# if EMILUA_CONFIG_THREAD_SUPPORT_LEVEL >= 1
#  include <condition_variable>
#  include <atomic>
#  include <thread>
#  include <mutex>
#  include <deque>
# endif // EMILUA_CONFIG_THREAD_SUPPORT_LEVEL >= 1
#else // BOOST_OS_WINDOWS
# include <boost/asio/posix/stream_descriptor.hpp>
#endif // BOOST_OS_WINDOWS

#if BOOST_OS_LINUX
#include <linux/close_range.h>
#include <sys/wait.h>
#endif // BOOST_OS_LINUX

namespace emilua {

char system_key;

static char system_signal_key;
static char system_signal_set_mt_key;
static char system_signal_set_wait_key;

#if !BOOST_OS_WINDOWS || EMILUA_CONFIG_THREAD_SUPPORT_LEVEL >= 1
static char system_in_key;
#endif // !BOOST_OS_WINDOWS || EMILUA_CONFIG_THREAD_SUPPORT_LEVEL >= 1
static char system_out_key;
static char system_err_key;

#if BOOST_OS_LINUX
static char subprocess_mt_key;
static char subprocess_wait_key;

struct spawn_arguments_t
{
    struct errno_reply_t
    {
        int code;
    };
    static_assert(sizeof(errno_reply_t) <= PIPE_BUF);

    bool use_path;
    int closeonexecpipe;
    const char* program;
    char** argv;
    char** envp;
    int proc_stdin;
    int proc_stdout;
    int proc_stderr;

    std::optional<sigset_t> signal_mask;
    std::optional<sigset_t> signal_default_handlers;
    std::optional<int> scheduler_policy;
    std::optional<int> scheduler_priority;
    bool start_new_session;
    std::optional<pid_t> process_group;
    bool resetids;
    std::optional<std::string> chdir;
};

struct spawn_reaper
{
    spawn_reaper(asio::io_context& ctx, int childpidfd, pid_t childpid,
                 int signal_on_zombie)
        : waiter{
            std::make_shared<asio::posix::stream_descriptor>(ctx, childpidfd)}
        , childpid{childpid}
        , signal_on_zombie{signal_on_zombie}
    {}

    std::shared_ptr<asio::posix::stream_descriptor> waiter;
    pid_t childpid;
    int signal_on_zombie;
};

struct subprocess
{
    subprocess()
        : info{std::in_place_type_t<void>{}}
    {}

    ~subprocess()
    {
        if (!reaper)
            return;

        if (reaper->signal_on_zombie != 0) {
            int res = kill(reaper->childpid, reaper->signal_on_zombie);
            boost::ignore_unused(res);
        }
        reaper->waiter->async_wait(
            asio::posix::descriptor_base::wait_read,
            [waiter=reaper->waiter](const boost::system::error_code& /*ec*/) {
                siginfo_t i;
                int res = waitid(P_PIDFD, waiter->native_handle(), &i, WEXITED);
                boost::ignore_unused(res);
            });
    }

    std::optional<spawn_reaper> reaper;
    bool wait_in_progress = false;
    result<siginfo_t, void> info;
};
#endif // BOOST_OS_LINUX

#if BOOST_OS_WINDOWS
# if EMILUA_CONFIG_THREAD_SUPPORT_LEVEL >= 1
// Windows cannot read from STD_INPUT_HANDLE using overlapped IO. Therefore we
// use a thread.
struct stdin_service: public pending_operation
{
    struct waiting_fiber
    {
        lua_State* fiber;
        std::shared_ptr<unsigned char[]> buffer;
        std::size_t buffer_size;
    };

    stdin_service(vm_context& vm_ctx);

    void cancel() noexcept override
    {
        run = false;

        {
            std::unique_lock<std::mutex> lk{queue_mtx};
            queue.clear();
            queue_is_not_empty_cond.notify_all(); //< forced spurious wakeup
        }

        lua_State* fiber = current_job;
        lua_State* expected = fiber;
        if (fiber && current_job.compare_exchange_strong(expected, NULL)) {
            do {
                expected = fiber;
                CancelSynchronousIo(thread.native_handle());
            } while (!current_job.compare_exchange_weak(expected, NULL));
        }

        thread.join();
    }

    std::deque<waiting_fiber> queue;
    std::mutex queue_mtx;
    std::condition_variable queue_is_not_empty_cond;
    std::atomic<lua_State*> current_job = nullptr;

    asio::executor_work_guard<asio::io_context::executor_type> work_guard;
    std::shared_ptr<vm_context> vm_ctx;

    std::atomic_bool run = true;
    std::thread thread;
};

inline stdin_service::stdin_service(vm_context& vm_ctx)
    : pending_operation{/*shared_ownership=*/false}
    , work_guard{vm_ctx.work_guard()}
    , thread{[this]() {
        HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
        while (run) {
            stdin_service::waiting_fiber fiber;
            {
                std::unique_lock<std::mutex> lk{queue_mtx};
                if (queue.empty())
                    this->vm_ctx.reset();
                while (queue.empty()) {
                    if (!run)
                        return;
                    queue_is_not_empty_cond.wait(lk);
                }
                assert(this->vm_ctx);

                fiber.fiber = queue.front().fiber;
                fiber.buffer = std::move(queue.front().buffer);
                fiber.buffer_size = queue.front().buffer_size;

                current_job = fiber.fiber;
                queue.pop_front();
            }

            DWORD numberOfBytesRead;
            BOOL ok = ReadFile(hStdin, fiber.buffer.get(), fiber.buffer_size,
                               &numberOfBytesRead, /*lpOverlapped=*/NULL);
            boost::system::error_code ec;
            if (!ok) {
                DWORD last_error = GetLastError();
                ec = boost::system::error_code(
                    last_error, asio::error::get_system_category());
            }

            lua_State* expected = fiber.fiber;
            if (!current_job.compare_exchange_strong(expected, NULL)) {
                assert(expected == NULL);
                current_job = fiber.fiber;
            }

            auto& v = this->vm_ctx;
            this->vm_ctx->strand().post(
                [vm_ctx=v,ec,numberOfBytesRead,fiber=fiber.fiber]() {
                    vm_ctx->fiber_resume(
                        fiber,
                        hana::make_set(
                            vm_context::options::auto_detect_interrupt,
                            hana::make_pair(
                                vm_context::options::arguments,
                                hana::make_tuple(ec, numberOfBytesRead))));
                }, std::allocator<void>{});
        }
    }}
{}
# endif // EMILUA_CONFIG_THREAD_SUPPORT_LEVEL >= 1
static int system_out_write_some(lua_State* L)
{
    // we don't really need to check for suspend-allowed here given no
    // fiber-switch happens and we block the whole thread, but it's better to do
    // it anyway to guarantee the function will behave the same across different
    // platforms
    auto& vm_ctx = get_vm_context(L);
    EMILUA_CHECK_SUSPEND_ALLOWED(vm_ctx, L);

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

    DWORD numberOfBytesWritten;
    BOOL ok = WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), bs->data.get(),
                        bs->size, &numberOfBytesWritten, /*lpOverlapped=*/NULL);
    if (!ok) {
        boost::system::error_code ec(
            GetLastError(), asio::error::get_system_category());
        push(L, ec);
        return lua_error(L);
    }

    lua_pushinteger(L, numberOfBytesWritten);
    return 1;
}

static int system_err_write_some(lua_State* L)
{
    // we don't really need to check for suspend-allowed here given no
    // fiber-switch happens and we block the whole thread, but it's better to do
    // it anyway to guarantee the function will behave the same across different
    // platforms
    auto& vm_ctx = get_vm_context(L);
    EMILUA_CHECK_SUSPEND_ALLOWED(vm_ctx, L);

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

    DWORD numberOfBytesWritten;
    BOOL ok = WriteFile(GetStdHandle(STD_ERROR_HANDLE), bs->data.get(),
                        bs->size, &numberOfBytesWritten, /*lpOverlapped=*/NULL);
    if (!ok) {
        boost::system::error_code ec(
            GetLastError(), asio::error::get_system_category());
        push(L, ec);
        return lua_error(L);
    }

    lua_pushinteger(L, numberOfBytesWritten);
    return 1;
}
#else // BOOST_OS_WINDOWS
struct stdstream_service: public pending_operation
{
    stdstream_service(asio::io_context& ioctx)
        : pending_operation{/*shared_ownership=*/false}
        , in{ioctx, STDIN_FILENO}
        , out{ioctx, STDOUT_FILENO}
        , err{ioctx, STDERR_FILENO}
    {}

    ~stdstream_service()
    {
        in.release();
        out.release();
        err.release();
    }

    void cancel() noexcept override
    {}

    asio::posix::stream_descriptor in;
    asio::posix::stream_descriptor out;
    asio::posix::stream_descriptor err;
};
#endif // BOOST_OS_WINDOWS

static int system_signal_set_new(lua_State* L)
{
    int nargs = lua_gettop(L);

    auto& vm_ctx = get_vm_context(L);
    if (!vm_ctx.is_master()) {
        push(L, std::errc::operation_not_permitted);
        return lua_error(L);
    }

    for (int i = 1 ; i <= nargs ; ++i) {
        if (lua_type(L, i) != LUA_TNUMBER) {
            push(L, std::errc::invalid_argument, "arg", i);
            return lua_error(L);
        }
    }

    auto set = static_cast<asio::signal_set*>(
        lua_newuserdata(L, sizeof(asio::signal_set))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &system_signal_set_mt_key);
    setmetatable(L, -2);
    new (set) asio::signal_set{vm_ctx.strand().context()};

    for (int i = 1 ; i <= nargs ; ++i) {
        boost::system::error_code ec;
        set->add(lua_tointeger(L, i), ec);
        if (ec) {
            push(L, ec, "arg", i);
            return lua_error(L);
        }
    }

    return 1;
}

static int system_signal_set_wait(lua_State* L)
{
    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto set = static_cast<asio::signal_set*>(lua_touserdata(L, 1));
    if (!set || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &system_signal_set_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto cancel_slot = set_default_interrupter(L, *vm_ctx);

    set->async_wait(
        asio::bind_cancellation_slot(cancel_slot, asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber](const boost::system::error_code& ec,
                                   int signal_number) {
                auto opt_args = vm_context::options::arguments;
                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            opt_args, hana::make_tuple(ec, signal_number))));
            }
        ))
    );

    return lua_yield(L, 0);
}

static int system_signal_set_add(lua_State* L)
{
    lua_settop(L, 2);

    auto set = static_cast<asio::signal_set*>(lua_touserdata(L, 1));
    if (!set || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &system_signal_set_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    if (lua_type(L, 2) != LUA_TNUMBER) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    boost::system::error_code ec;
    set->add(lua_tointeger(L, 2), ec);
    if (ec) {
        push(L, ec);
        return lua_error(L);
    }
    return 0;
}

static int system_signal_set_remove(lua_State* L)
{
    lua_settop(L, 2);

    auto set = static_cast<asio::signal_set*>(lua_touserdata(L, 1));
    if (!set || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &system_signal_set_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    if (lua_type(L, 2) != LUA_TNUMBER) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    boost::system::error_code ec;
    set->remove(lua_tointeger(L, 2), ec);
    if (ec) {
        push(L, ec);
        return lua_error(L);
    }
    return 0;
}

static int system_signal_set_clear(lua_State* L)
{
    auto set = static_cast<asio::signal_set*>(lua_touserdata(L, 1));
    if (!set || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &system_signal_set_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    set->clear(ec);
    if (ec) {
        push(L, ec);
        return lua_error(L);
    }
    return 0;
}

static int system_signal_set_cancel(lua_State* L)
{
    auto set = static_cast<asio::signal_set*>(lua_touserdata(L, 1));
    if (!set || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &system_signal_set_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    set->cancel(ec);
    if (ec) {
        push(L, ec);
        return lua_error(L);
    }
    return 0;
}

static int system_signal_set_mt_index(lua_State* L)
{
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("wait"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX, &system_signal_set_wait_key);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("cancel"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, system_signal_set_cancel);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("add"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, system_signal_set_add);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("remove"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, system_signal_set_remove);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("clear"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, system_signal_set_clear);
                    return 1;
                }
            )
        ),
        [](std::string_view /*key*/, lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        },
        tostringview(L, 2),
        L
    );
}

static int system_signal_raise(lua_State* L)
{
    int signal_number = luaL_checkinteger(L, 1);

#define EMILUA_DETAIL_IS_SIG_X_OR(SIG) signal_number == SIG ||
#define EMILUA_IS_SIG_X_OR(SIG) BOOST_PP_IIF( \
        BOOST_VMD_IS_NUMBER(SIG), EMILUA_DETAIL_IS_SIG_X_OR, BOOST_VMD_EMPTY \
    )(SIG)

#define EMILUA_DETAIL_IS_NOT_SIG_X_AND(SIG) signal_number != SIG &&
#define EMILUA_IS_NOT_SIG_X_AND(SIG) BOOST_PP_IIF( \
        BOOST_VMD_IS_NUMBER(SIG), \
        EMILUA_DETAIL_IS_NOT_SIG_X_AND, BOOST_VMD_EMPTY \
    )(SIG)

    auto& vm_ctx = get_vm_context(L);
    if (!vm_ctx.is_master()) {
        // SIGKILL and SIGSTOP are the only signals that cannot be caught,
        // blocked, or ignored. If we allowed any child VM to raise these
        // signals, then the protection to only allow the main VM to force-exit
        // the process would be moot.
        if (
            EMILUA_IS_SIG_X_OR(SIGKILL)
            EMILUA_IS_SIG_X_OR(SIGSTOP)
            false
        ) {
            push(L, std::errc::operation_not_permitted);
            return lua_error(L);
        }

        // Unless the main VM has a handler installed (the check doesn't need to
        // be race-free... that's not a problem) for the process-terminating
        // signal, forbid slave VMs from raising it.
        if (
            // Default action is to continue the process (whatever lol)
            EMILUA_IS_NOT_SIG_X_AND(SIGCONT)

            // Default action is to ignore the signal
            EMILUA_IS_NOT_SIG_X_AND(SIGCHLD)
            EMILUA_IS_NOT_SIG_X_AND(SIGURG)
            EMILUA_IS_NOT_SIG_X_AND(SIGWINCH)

            true
        ) {
#ifdef _POSIX_C_SOURCE
            struct sigaction curact;
            if (
                sigaction(signal_number, /*act=*/NULL, /*oldact=*/&curact) == -1
            ) {
                push(L, std::error_code{errno, std::system_category()});
                return lua_error(L);
            }

            if (curact.sa_handler == SIG_DFL) {
                push(L, std::errc::operation_not_permitted);
                return lua_error(L);
            }
#else
            // TODO: a Windows implementation that checks for SIG_DFL
            push(L, std::errc::operation_not_permitted);
            return lua_error(L);
#endif // _POSIX_C_SOURCE
        }
    }

#undef EMILUA_IS_NOT_SIG_X_AND
#undef EMILUA_DETAIL_IS_NOT_SIG_X_AND
#undef EMILUA_IS_SIG_X_OR
#undef EMILUA_DETAIL_IS_SIG_X_OR

    int ret = std::raise(signal_number);
    if (ret != 0) {
        push(L, errc::raise_error, "ret", ret);
        return lua_error(L);
    }
    return 0;
}

#if BOOST_OS_WINDOWS
# if EMILUA_CONFIG_THREAD_SUPPORT_LEVEL >= 1
static int system_in_read_some(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);
    EMILUA_CHECK_SUSPEND_ALLOWED(vm_ctx, L);

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

    stdin_service::waiting_fiber operation;
    operation.fiber = vm_ctx.current_fiber();
    operation.buffer = bs->data;
    operation.buffer_size = bs->size;

    stdin_service* service = nullptr;

    for (auto& op: vm_ctx.pending_operations) {
        service = dynamic_cast<stdin_service*>(&op);
        if (service) {
            std::unique_lock<std::mutex> lk(service->queue_mtx);
            if (service->queue.empty() && !service->vm_ctx)
                service->vm_ctx = vm_ctx.shared_from_this();
            service->queue.push_back(operation);

            if (service->queue.size() == 1)
                service->queue_is_not_empty_cond.notify_all();
            break;
        }
    }

    if (!service) {
        service = new stdin_service{vm_ctx};
        vm_ctx.pending_operations.push_back(*service);

        std::unique_lock<std::mutex> lk{service->queue_mtx};
        service->vm_ctx = vm_ctx.shared_from_this();
        service->queue.push_back(operation);
        service->queue_is_not_empty_cond.notify_all();
    }

    lua_pushlightuserdata(L, service);
    lua_pushlightuserdata(L, vm_ctx.current_fiber());
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto service = static_cast<stdin_service*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            auto fiber = static_cast<lua_State*>(
                lua_touserdata(L, lua_upvalueindex(2)));

            auto vm_ctx = get_vm_context(L).shared_from_this();

            {
                std::unique_lock<std::mutex> lk(service->queue_mtx);
                auto it = service->queue.begin();
                auto end = service->queue.end();
                for (; it != end ; ++it) {
                    if (it->fiber == fiber) {
                        service->queue.erase(it);
                        vm_ctx->strand().post(
                            [vm_ctx,fiber]() {
                                auto ec = make_error_code(errc::interrupted);
                                vm_ctx->fiber_resume(
                                    fiber,
                                    hana::make_set(
                                        hana::make_pair(
                                            vm_context::options::arguments,
                                            hana::make_tuple(ec))));
                            }, std::allocator<void>{});
                        return 0;
                    }
                }

                lua_State* expected = fiber;
                if (
                    service->current_job.compare_exchange_strong(expected, NULL)
                ) {
                    do {
                        expected = fiber;
                        CancelSynchronousIo(service->thread.native_handle());
                    } while (!service->current_job.compare_exchange_weak(
                        expected, NULL
                    ));
                }
            }

            return 0;
        },
        2);
    set_interrupter(L, vm_ctx);

    return lua_yield(L, 0);
}
# endif // EMILUA_CONFIG_THREAD_SUPPORT_LEVEL >= 1
#else // BOOST_OS_WINDOWS
static int system_in_read_some(lua_State* L)
{
    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

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

    stdstream_service* service = nullptr;

    for (auto& op: vm_ctx->pending_operations) {
        service = dynamic_cast<stdstream_service*>(&op);
        if (service)
            break;
    }

    if (!service) {
        service = new stdstream_service{vm_ctx->strand().context()};
        vm_ctx->pending_operations.push_back(*service);
    }

    auto cancel_slot = set_default_interrupter(L, *vm_ctx);

    service->in.async_read_some(
        asio::buffer(bs->data.get(), bs->size),
        asio::bind_cancellation_slot(cancel_slot, asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,buf=bs->data](
                const boost::system::error_code& ec,
                std::size_t bytes_transferred
            ) {
                std::error_code ec2 = ec;
                if (ec2 == std::errc::interrupted)
                    ec2 = errc::interrupted;
                boost::ignore_unused(buf);
                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::fast_auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(ec2, bytes_transferred)))
                );
            }
        ))
    );

    return lua_yield(L, 0);
}

static int system_out_write_some(lua_State* L)
{
    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

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

    stdstream_service* service = nullptr;

    for (auto& op: vm_ctx->pending_operations) {
        service = dynamic_cast<stdstream_service*>(&op);
        if (service)
            break;
    }

    if (!service) {
        service = new stdstream_service{vm_ctx->strand().context()};
        vm_ctx->pending_operations.push_back(*service);
    }

    auto cancel_slot = set_default_interrupter(L, *vm_ctx);

    service->out.async_write_some(
        asio::buffer(bs->data.get(), bs->size),
        asio::bind_cancellation_slot(cancel_slot, asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,buf=bs->data](
                const boost::system::error_code& ec,
                std::size_t bytes_transferred
            ) {
                std::error_code ec2 = ec;
                if (ec2 == std::errc::interrupted)
                    ec2 = errc::interrupted;
                boost::ignore_unused(buf);
                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::fast_auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(ec2, bytes_transferred)))
                );
            }
        ))
    );

    return lua_yield(L, 0);
}

static int system_err_write_some(lua_State* L)
{
    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

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

    stdstream_service* service = nullptr;

    for (auto& op: vm_ctx->pending_operations) {
        service = dynamic_cast<stdstream_service*>(&op);
        if (service)
            break;
    }

    if (!service) {
        service = new stdstream_service{vm_ctx->strand().context()};
        vm_ctx->pending_operations.push_back(*service);
    }

    auto cancel_slot = set_default_interrupter(L, *vm_ctx);

    service->err.async_write_some(
        asio::buffer(bs->data.get(), bs->size),
        asio::bind_cancellation_slot(cancel_slot, asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,buf=bs->data](
                const boost::system::error_code& ec,
                std::size_t bytes_transferred
            ) {
                std::error_code ec2 = ec;
                if (ec2 == std::errc::interrupted)
                    ec2 = errc::interrupted;
                boost::ignore_unused(buf);
                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::fast_auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(ec2, bytes_transferred)))
                );
            }
        ))
    );

    return lua_yield(L, 0);
}
#endif // BOOST_OS_WINDOWS

inline int system_arguments(lua_State* L)
{
    auto& appctx = get_vm_context(L).appctx;
    lua_createtable(L, /*narr=*/appctx.app_args.size(), /*nrec=*/0);
    int n = 0;
    for (auto& arg: appctx.app_args) {
        push(L, arg);
        lua_rawseti(L, -2, ++n);
    }
    return 1;
}

inline int system_environment(lua_State* L)
{
    auto& appctx = get_vm_context(L).appctx;
    lua_createtable(L, /*narr=*/0, /*nrec=*/appctx.app_env.size());
    for (auto& [key, value]: appctx.app_env) {
        push(L, key);
        push(L, value);
        lua_rawset(L, -3);
    }
    return 1;
}

#if BOOST_OS_UNIX
template<int FD>
static int system_stdhandle_dup(lua_State* L)
{
    int newfd = dup(FD);
    BOOST_SCOPE_EXIT_ALL(&) {
        if (newfd != -1) {
            int res = close(newfd);
            boost::ignore_unused(res);
        }
    };
    if (newfd == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }

    auto newhandle = static_cast<file_descriptor_handle*>(
        lua_newuserdata(L, sizeof(file_descriptor_handle))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
    setmetatable(L, -2);

    *newhandle = newfd;
    newfd = -1;
    return 1;
}
#endif // BOOST_OS_UNIX

#if !BOOST_OS_WINDOWS || EMILUA_CONFIG_THREAD_SUPPORT_LEVEL >= 1
inline int system_in(lua_State* L)
{
    rawgetp(L, LUA_REGISTRYINDEX, &system_in_key);
    return 1;
}
#endif // !BOOST_OS_WINDOWS || EMILUA_CONFIG_THREAD_SUPPORT_LEVEL >= 1

inline int system_out(lua_State* L)
{
    rawgetp(L, LUA_REGISTRYINDEX, &system_out_key);
    return 1;
}

inline int system_err(lua_State* L)
{
    rawgetp(L, LUA_REGISTRYINDEX, &system_err_key);
    return 1;
}

inline int system_signal(lua_State* L)
{
    rawgetp(L, LUA_REGISTRYINDEX, &system_signal_key);
    return 1;
}

static int system_exit(lua_State* L)
{
    lua_settop(L, 2);

    int exit_code = luaL_optint(L, 1, EXIT_SUCCESS);

    auto& vm_ctx = get_vm_context(L);
    if (vm_ctx.is_master()) {
        if (lua_type(L, 2) == LUA_TTABLE) {
            lua_getfield(L, 2, "force");
            switch (lua_type(L, -1)) {
            case LUA_TNIL:
                break;
            case LUA_TNUMBER:
                switch (lua_tointeger(L, -1)) {
                case 0:
                    break;
                case 1:
                    push(L, std::errc::not_supported);
                    return lua_error(L);
                case 2:
#if BOOST_OS_MACOS
                    std::_Exit(exit_code);
#else // BOOST_OS_MACOS
                    std::quick_exit(exit_code);
#endif // BOOST_OS_MACOS
                default:
                    push(L, std::errc::invalid_argument, "arg", "force");
                    return lua_error(L);
                }
                break;
            case LUA_TSTRING: {
                auto force = tostringview(L);
                if (force == "abort") {
                    std::abort();
                } else {
                    push(L, std::errc::invalid_argument, "arg", "force");
                    return lua_error(L);
                }
            }
            default:
                push(L, std::errc::invalid_argument, "arg", 2);
                return lua_error(L);
            }
        }

        vm_ctx.appctx.exit_code = exit_code;
    } else if (lua_type(L, 2) != LUA_TNIL) {
        push(L, std::errc::operation_not_permitted);
        return lua_error(L);
    }

    vm_ctx.notify_exit_request();
    return lua_yield(L, 0);
}

#if BOOST_OS_UNIX
static int system_getresuid(lua_State* L)
{
    uid_t ruid, euid, suid;
    int res = getresuid(&ruid, &euid, &suid);
    assert(res == 0);
    boost::ignore_unused(res);
    lua_pushinteger(L, ruid);
    lua_pushinteger(L, euid);
    lua_pushinteger(L, suid);
    return 3;
}

static int system_getresgid(lua_State* L)
{
    gid_t rgid, egid, sgid;
    int res = getresgid(&rgid, &egid, &sgid);
    assert(res == 0);
    boost::ignore_unused(res);
    lua_pushinteger(L, rgid);
    lua_pushinteger(L, egid);
    lua_pushinteger(L, sgid);
    return 3;
}

static int system_setresuid(lua_State* L)
{
    lua_settop(L, 3);
    auto& vm_ctx = get_vm_context(L);
    if (!vm_ctx.is_master()) {
        push(L, std::errc::operation_not_permitted);
        return lua_error(L);
    }

    uid_t ruid = lua_tointeger(L, 1);
    uid_t euid = lua_tointeger(L, 2);
    uid_t suid = lua_tointeger(L, 3);
    if (setresuid(ruid, euid, suid) == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }
    return 0;
}

static int system_setresgid(lua_State* L)
{
    lua_settop(L, 3);
    auto& vm_ctx = get_vm_context(L);
    if (!vm_ctx.is_master()) {
        push(L, std::errc::operation_not_permitted);
        return lua_error(L);
    }

    gid_t rgid = lua_tointeger(L, 1);
    gid_t egid = lua_tointeger(L, 2);
    gid_t sgid = lua_tointeger(L, 3);
    if (setresgid(rgid, egid, sgid) == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }
    return 0;
}
#endif // BOOST_OS_UNIX

#if BOOST_OS_LINUX
static int subprocess_wait(lua_State* L)
{
    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto p = static_cast<subprocess*>(lua_touserdata(L, 1));
    if (!p || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &subprocess_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    if (!p->reaper) {
        push(L, std::errc::no_child_process);
        return lua_error(L);
    }

    if (p->wait_in_progress) {
        push(L, std::errc::device_or_resource_busy);
        return lua_error(L);
    }

    p->wait_in_progress = true;

    lua_pushvalue(L, 1);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto p = static_cast<subprocess*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            boost::system::error_code ignored_ec;
            p->reaper->waiter->cancel(ignored_ec);
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    p->reaper->waiter->async_wait(
        asio::posix::descriptor_base::wait_read,
        asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [waiter=p->reaper->waiter,vm_ctx,current_fiber,p](
                const boost::system::error_code& ec
            ) {
                if (vm_ctx->valid())
                    p->wait_in_progress = false;

                if (ec) {
                    assert(ec == asio::error::operation_aborted);
                    auto opt_args = vm_context::options::arguments;
                    vm_ctx->fiber_resume(
                        current_fiber,
                        hana::make_set(
                            vm_context::options::fast_auto_detect_interrupt,
                            hana::make_pair(opt_args, hana::make_tuple(ec))));
                    return;
                }

                if (!vm_ctx->valid())
                    return;

                siginfo_t i;
                int res = waitid(P_PIDFD, waiter->native_handle(), &i, WEXITED);
                boost::ignore_unused(res);
                p->info = i;

                p->reaper = std::nullopt;

                vm_ctx->fiber_resume(current_fiber);
            }
        )
    );

    return lua_yield(L, 0);
}

static int subprocess_kill(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TNUMBER);

    auto p = static_cast<subprocess*>(lua_touserdata(L, 1));
    if (!p || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &subprocess_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    if (!p->reaper) {
        push(L, std::errc::no_such_process);
        return lua_error(L);
    }

    if (kill(p->reaper->childpid, lua_tointeger(L, 2)) == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }

    return 0;
}

inline int subprocess_exit_code(lua_State* L)
{
    auto p = static_cast<subprocess*>(lua_touserdata(L, 1));
    if (!p->info) {
        push(L, std::errc::invalid_argument);
        return lua_error(L);
    }
    switch (p->info.value().si_code) {
    default:
        assert(false);
        push(L, std::errc::state_not_recoverable);
        return lua_error(L);
    case CLD_EXITED:
        lua_pushinteger(L, p->info.value().si_status);
        return 1;
    case CLD_KILLED:
    case CLD_DUMPED:
        lua_pushinteger(L, 128 + p->info.value().si_status);
        return 1;
    }
}

inline int subprocess_exit_signal(lua_State* L)
{
    auto p = static_cast<subprocess*>(lua_touserdata(L, 1));
    if (!p->info) {
        push(L, std::errc::invalid_argument);
        return lua_error(L);
    }
    switch (p->info.value().si_code) {
    default:
        lua_pushnil(L);
        return 1;
    case CLD_KILLED:
    case CLD_DUMPED:
        lua_pushinteger(L, p->info.value().si_status);
        return 1;
    }
}

inline int subprocess_pid(lua_State* L)
{
    auto p = static_cast<subprocess*>(lua_touserdata(L, 1));
    if (!p->reaper) {
        push(L, std::errc::no_child_process);
        return lua_error(L);
    }

    lua_pushinteger(L, p->reaper->childpid);
    return 1;
}

static int subprocess_mt_index(lua_State* L)
{
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("wait"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX, &subprocess_wait_key);
                    return 1;
                }),
            hana::make_pair(
                BOOST_HANA_STRING("kill"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, subprocess_kill);
                    return 1;
                }),
            hana::make_pair(
                BOOST_HANA_STRING("exit_code"), subprocess_exit_code),
            hana::make_pair(
                BOOST_HANA_STRING("exit_signal"), subprocess_exit_signal),
            hana::make_pair(BOOST_HANA_STRING("pid"), subprocess_pid)
        ),
        [](std::string_view /*key*/, lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        },
        tostringview(L, 2),
        L
    );
}

static int system_spawn_child_main(void* a)
{
    auto args = reinterpret_cast<spawn_arguments_t*>(a);
    spawn_arguments_t::errno_reply_t reply;

    if (args->signal_mask) {
        int res = sigprocmask(SIG_SETMASK, &*args->signal_mask,
                              /*oldset=*/NULL);
        boost::ignore_unused(res);
    }

    if (args->signal_default_handlers) {
        struct sigaction sa;
        std::memset(&sa, 0, sizeof(sa));
        for (int signo = 1 ; signo != NSIG ; ++signo) {
            if (sigismember(&*args->signal_default_handlers, signo) == 1) {
                sa.sa_handler = SIG_DFL;
                int res = sigaction(signo, /*act=*/&sa, /*oldact=*/NULL);
                boost::ignore_unused(res);
            }
        }
    }

    if (args->scheduler_policy) {
        struct sched_param sp;
        sp.sched_priority = *args->scheduler_priority;
        if (sched_setscheduler(/*pid=*/0, *args->scheduler_policy, &sp) == -1) {
            reply.code = errno;
            write(args->closeonexecpipe, &reply, sizeof(reply));
            return 1;
        }
    } else if (args->scheduler_priority) {
        struct sched_param sp;
        sp.sched_priority = *args->scheduler_priority;
        if (sched_setparam(/*pid=*/0, &sp) == -1) {
            reply.code = errno;
            write(args->closeonexecpipe, &reply, sizeof(reply));
            return 1;
        }
    }

    if (args->start_new_session && setsid() == -1) {
        reply.code = errno;
        write(args->closeonexecpipe, &reply, sizeof(reply));
        return 1;
    }

    if (args->process_group && setpgid(/*pid=*/0, *args->process_group) == -1) {
        reply.code = errno;
        write(args->closeonexecpipe, &reply, sizeof(reply));
        return 1;
    }

    if (
        args->resetids && (seteuid(getuid()) == -1 || setegid(getgid()) == -1)
    ) {
        reply.code = errno;
        write(args->closeonexecpipe, &reply, sizeof(reply));
        return 1;
    }

    if (args->chdir && chdir(args->chdir->data()) == -1) {
        reply.code = errno;
        write(args->closeonexecpipe, &reply, sizeof(reply));
        return 1;
    }

    switch (args->proc_stdin) {
    case -1: {
        int pipefd[2];
        if (pipe(pipefd) == -1) {
            reply.code = errno;
            write(args->closeonexecpipe, &reply, sizeof(reply));
            return 1;
        }
        if (dup2(pipefd[0], STDIN_FILENO) == -1) {
            reply.code = errno;
            write(args->closeonexecpipe, &reply, sizeof(reply));
            return 1;
        }
        break;
    }
    case STDIN_FILENO:
        break;
    default:
        if (dup2(args->proc_stdin, STDIN_FILENO) == -1) {
            reply.code = errno;
            write(args->closeonexecpipe, &reply, sizeof(reply));
            return 1;
        }
    }

    switch (args->proc_stdout) {
    case -1: {
        int pipefd[2];
        if (pipe(pipefd) == -1) {
            reply.code = errno;
            write(args->closeonexecpipe, &reply, sizeof(reply));
            return 1;
        }
        if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
            reply.code = errno;
            write(args->closeonexecpipe, &reply, sizeof(reply));
            return 1;
        }
        break;
    }
    case STDOUT_FILENO:
        break;
    default:
        if (dup2(args->proc_stdout, STDOUT_FILENO) == -1) {
            reply.code = errno;
            write(args->closeonexecpipe, &reply, sizeof(reply));
            return 1;
        }
    }

    switch (args->proc_stderr) {
    case -1: {
        int pipefd[2];
        if (pipe(pipefd) == -1) {
            reply.code = errno;
            write(args->closeonexecpipe, &reply, sizeof(reply));
            return 1;
        }
        if (dup2(pipefd[1], STDERR_FILENO) == -1) {
            reply.code = errno;
            write(args->closeonexecpipe, &reply, sizeof(reply));
            return 1;
        }
        break;
    }
    case STDERR_FILENO:
        break;
    default:
        if (dup2(args->proc_stderr, STDERR_FILENO) == -1) {
            reply.code = errno;
            write(args->closeonexecpipe, &reply, sizeof(reply));
            return 1;
        }
    }

    if (dup3(args->closeonexecpipe, 3, O_CLOEXEC) == -1) {
        reply.code = errno;
        write(args->closeonexecpipe, &reply, sizeof(reply));
        return 1;
    }

    if (close_range(/*first=*/4, /*last=*/~0U, /*flags=*/0) == -1) {
        reply.code = errno;
        write(3, &reply, sizeof(reply));
        return 1;
    }

    if (args->use_path)
        execvpe(args->program, args->argv, args->envp);
    else
        execve(args->program, args->argv, args->envp);

    reply.code = errno;
    write(3, &reply, sizeof(reply));
    return 1;
}

static int system_spawn_do(bool use_path, lua_State* L)
{
    lua_settop(L, 1);
    luaL_checktype(L, 1, LUA_TTABLE);
    auto& vm_ctx = get_vm_context(L);
    rawgetp(L, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
    const int FILE_DESCRIPTOR_MT_INDEX = lua_gettop(L);

    lua_getfield(L, 1, "program");
    if (lua_type(L, -1) != LUA_TSTRING) {
        push(L, std::errc::invalid_argument, "arg", "program");
        return lua_error(L);
    }
    std::string program{tostringview(L)};
    lua_pop(L, 1);

    int signal_on_zombie = SIGTERM;
    lua_getfield(L, 1, "signal_on_zombie");
    switch (lua_type(L, -1)) {
    case LUA_TNIL:
        break;
    case LUA_TNUMBER:
        signal_on_zombie = lua_tointeger(L, -1);
        break;
    default:
        push(L, std::errc::invalid_argument, "arg", "signal_on_zombie");
        return lua_error(L);
    }
    lua_pop(L, 1);

    std::vector<std::string> arguments;
    lua_getfield(L, 1, "arguments");
    switch (lua_type(L, -1)) {
    case LUA_TNIL:
        break;
    case LUA_TTABLE:
        for (int i = 1 ;; ++i) {
            lua_rawgeti(L, -1, i);
            switch (lua_type(L, -1)) {
            case LUA_TNIL:
                lua_pop(L, 1);
                goto end_for;
            case LUA_TSTRING:
                arguments.emplace_back(tostringview(L));
                lua_pop(L, 1);
                break;
            default:
                push(L, std::errc::invalid_argument, "arg", "arguments");
                return lua_error(L);
            }
        }
        end_for:
        break;
    default:
        push(L, std::errc::invalid_argument, "arg", "arguments");
        return lua_error(L);
    }
    lua_pop(L, 1);
    std::vector<char*> argumentsb;
    argumentsb.reserve(arguments.size() + 1);
    for (auto& a: arguments) {
        argumentsb.emplace_back(a.data());
    }
    argumentsb.emplace_back(nullptr);

    std::vector<std::string> environment;
    lua_getfield(L, 1, "environment");
    switch (lua_type(L, -1)) {
    case LUA_TNIL:
        break;
    case LUA_TTABLE:
        lua_pushnil(L);
        while (lua_next(L, -2) != 0) {
            if (
                lua_type(L, -2) != LUA_TSTRING || lua_type(L, -1) != LUA_TSTRING
            ) {
                push(L, std::errc::invalid_argument, "arg", "environment");
                return lua_error(L);
            }

            environment.emplace_back(tostringview(L, -2));
            environment.back() += '=';
            environment.back() += tostringview(L, -1);
            lua_pop(L, 1);
        }
        break;
    default:
        push(L, std::errc::invalid_argument, "arg", "environment");
        return lua_error(L);
    }
    lua_pop(L, 1);
    std::vector<char*> environmentb;
    environmentb.reserve(environment.size() + 1);
    for (auto& e: environment) {
        environmentb.emplace_back(e.data());
    }
    environmentb.emplace_back(nullptr);

    int proc_stdin = -1;
    lua_getfield(L, 1, "stdin");
    switch (lua_type(L, -1)) {
    case LUA_TNIL:
        break;
    case LUA_TSTRING:
        if (tostringview(L) != "share") {
            push(L, std::errc::invalid_argument, "arg", "stdin");
            return lua_error(L);
        }
        proc_stdin = STDIN_FILENO;
        break;
    case LUA_TUSERDATA: {
        auto handle = static_cast<file_descriptor_handle*>(
            lua_touserdata(L, -1));
        if (!lua_getmetatable(L, -1)) {
            push(L, std::errc::invalid_argument, "arg", "stdin");
            return lua_error(L);
        }
        if (!lua_rawequal(L, -1, FILE_DESCRIPTOR_MT_INDEX)) {
            push(L, std::errc::invalid_argument, "arg", "stdin");
            return lua_error(L);
        }
        lua_pop(L, 1);
        if (*handle == INVALID_FILE_DESCRIPTOR) {
            push(L, std::errc::device_or_resource_busy, "arg", "stdin");
            return lua_error(L);
        }
        proc_stdin = *handle;
        break;
    }
    default:
        push(L, std::errc::invalid_argument, "arg", "stdin");
        return lua_error(L);
    }
    lua_pop(L, 1);

    int proc_stdout = -1;
    lua_getfield(L, 1, "stdout");
    switch (lua_type(L, -1)) {
    case LUA_TNIL:
        break;
    case LUA_TSTRING:
        if (tostringview(L) != "share") {
            push(L, std::errc::invalid_argument, "arg", "stdout");
            return lua_error(L);
        }
        proc_stdout = STDOUT_FILENO;
        break;
    case LUA_TUSERDATA: {
        auto handle = static_cast<file_descriptor_handle*>(
            lua_touserdata(L, -1));
        if (!lua_getmetatable(L, -1)) {
            push(L, std::errc::invalid_argument, "arg", "stdout");
            return lua_error(L);
        }
        if (!lua_rawequal(L, -1, FILE_DESCRIPTOR_MT_INDEX)) {
            push(L, std::errc::invalid_argument, "arg", "stdout");
            return lua_error(L);
        }
        lua_pop(L, 1);
        if (*handle == INVALID_FILE_DESCRIPTOR) {
            push(L, std::errc::device_or_resource_busy, "arg", "stdout");
            return lua_error(L);
        }
        proc_stdout = *handle;
        break;
    }
    default:
        push(L, std::errc::invalid_argument, "arg", "stdout");
        return lua_error(L);
    }
    lua_pop(L, 1);

    int proc_stderr = -1;
    lua_getfield(L, 1, "stderr");
    switch (lua_type(L, -1)) {
    case LUA_TNIL:
        break;
    case LUA_TSTRING:
        if (tostringview(L) != "share") {
            push(L, std::errc::invalid_argument, "arg", "stderr");
            return lua_error(L);
        }
        proc_stderr = STDERR_FILENO;
        break;
    case LUA_TUSERDATA: {
        auto handle = static_cast<file_descriptor_handle*>(
            lua_touserdata(L, -1));
        if (!lua_getmetatable(L, -1)) {
            push(L, std::errc::invalid_argument, "arg", "stderr");
            return lua_error(L);
        }
        if (!lua_rawequal(L, -1, FILE_DESCRIPTOR_MT_INDEX)) {
            push(L, std::errc::invalid_argument, "arg", "stderr");
            return lua_error(L);
        }
        lua_pop(L, 1);
        if (*handle == INVALID_FILE_DESCRIPTOR) {
            push(L, std::errc::device_or_resource_busy, "arg", "stderr");
            return lua_error(L);
        }
        proc_stderr = *handle;
        break;
    }
    default:
        push(L, std::errc::invalid_argument, "arg", "stderr");
        return lua_error(L);
    }
    lua_pop(L, 1);

    std::optional<sigset_t> signal_mask;
    lua_getfield(L, 1, "signal_mask");
    switch (lua_type(L, -1)) {
    case LUA_TNIL:
        break;
    case LUA_TTABLE:
        sigemptyset(&signal_mask.emplace());
        for (int i = 1 ;; ++i) {
            lua_rawgeti(L, -1, i);
            switch (lua_type(L, -1)) {
            case LUA_TNIL:
                lua_pop(L, 1);
                goto end_for2;
            case LUA_TNUMBER:
                if (sigaddset(&*signal_mask, lua_tointeger(L, -1)) == -1) {
                    push(L, std::error_code{errno, std::system_category()});
                    return lua_error(L);
                }
                lua_pop(L, 1);
                break;
            default:
                push(L, std::errc::invalid_argument, "arg", "signal_mask");
                return lua_error(L);
            }
        }
        end_for2:
        break;
    default:
        push(L, std::errc::invalid_argument, "arg", "signal_mask");
        return lua_error(L);
    }
    lua_pop(L, 1);

    std::optional<sigset_t> signal_default_handlers;
    lua_getfield(L, 1, "signal_default_handlers");
    switch (lua_type(L, -1)) {
    case LUA_TNIL:
        break;
    case LUA_TTABLE:
        sigemptyset(&signal_default_handlers.emplace());
        for (int i = 1 ;; ++i) {
            lua_rawgeti(L, -1, i);
            switch (lua_type(L, -1)) {
            case LUA_TNIL:
                lua_pop(L, 1);
                goto end_for3;
            case LUA_TNUMBER: {
                int signo = lua_tointeger(L, -1);
                if (signo == SIGKILL || signo == SIGSTOP) {
                    push(L, std::errc::invalid_argument,
                         "arg", "signal_default_handlers");
                    return lua_error(L);
                }
                if (sigaddset(&*signal_default_handlers, signo) == -1) {
                    push(L, std::error_code{errno, std::system_category()});
                    return lua_error(L);
                }
                lua_pop(L, 1);
                break;
            }
            default:
                push(L, std::errc::invalid_argument,
                     "arg", "signal_default_handlers");
                return lua_error(L);
            }
        }
        end_for3:
        break;
    default:
        push(L, std::errc::invalid_argument, "arg", "signal_default_handlers");
        return lua_error(L);
    }
    lua_pop(L, 1);

    std::optional<int> scheduler_policy;
    std::optional<int> scheduler_priority;
    lua_getfield(L, 1, "scheduler");
    switch (lua_type(L, -1)) {
    case LUA_TNIL:
        break;
    case LUA_TTABLE:
        lua_getfield(L, -1, "policy");
        switch (lua_type(L, -1)) {
        case LUA_TNIL:
            break;
        case LUA_TSTRING: {
            auto policy = tostringview(L);
            if (policy == "other") {
                scheduler_policy.emplace(SCHED_OTHER);
            } else if (policy == "batch") {
                scheduler_policy.emplace(SCHED_BATCH);
            } else if (policy == "idle") {
                scheduler_policy.emplace(SCHED_IDLE);
            } else if (policy == "fifo") {
                scheduler_policy.emplace(SCHED_FIFO);
            } else if (policy == "rr") {
                scheduler_policy.emplace(SCHED_RR);
            } else {
                push(L, std::errc::invalid_argument, "arg", "scheduler.policy");
                return lua_error(L);
            }
            break;
        }
        default:
            push(L, std::errc::invalid_argument, "arg", "scheduler.policy");
            return lua_error(L);
        }
        lua_pop(L, 1);

        lua_getfield(L, -1, "reset_on_fork");
        switch (lua_type(L, -1)) {
        case LUA_TNIL:
            break;
        case LUA_TBOOLEAN:
            if (!scheduler_policy) {
                push(L, std::errc::invalid_argument,
                     "arg", "scheduler.reset_on_fork");
                return lua_error(L);
            }

            if (!lua_toboolean(L, -1))
                break;

            *scheduler_policy |= SCHED_RESET_ON_FORK;
            break;
        default:
            push(L, std::errc::invalid_argument,
                 "arg", "scheduler.reset_on_fork");
            return lua_error(L);
        }
        lua_pop(L, 1);

        lua_getfield(L, -1, "priority");
        switch (lua_type(L, -1)) {
        case LUA_TNIL:
            break;
        case LUA_TNUMBER:
            scheduler_priority.emplace(lua_tointeger(L, -1));
            break;
        default:
            push(L, std::errc::invalid_argument, "arg", "scheduler.priority");
            return lua_error(L);
        }
        lua_pop(L, 1);
        break;
    default:
        push(L, std::errc::invalid_argument, "arg", "scheduler");
        return lua_error(L);
    }
    lua_pop(L, 1);

    if (scheduler_policy) {
        switch (
            int policy = *scheduler_policy & ~SCHED_RESET_ON_FORK ; policy
        ) {
        case SCHED_OTHER:
        case SCHED_BATCH:
        case SCHED_IDLE:
            if (scheduler_priority && *scheduler_priority != 0) {
                push(L, std::errc::invalid_argument,
                     "arg", "scheduler.priority");
                return lua_error(L);
            }
            scheduler_priority.emplace(0);
            break;
        case SCHED_FIFO:
        case SCHED_RR:
            if (!scheduler_priority) {
                push(L, std::errc::invalid_argument,
                     "arg", "scheduler.priority");
                return lua_error(L);
            }

            if (
                *scheduler_priority < sched_get_priority_min(policy) ||
                *scheduler_priority > sched_get_priority_max(policy)
            ) {
                push(L, std::errc::invalid_argument,
                     "arg", "scheduler.priority");
                return lua_error(L);
            }
        }
    }

    bool start_new_session = false;
    lua_getfield(L, 1, "start_new_session");
    switch (lua_type(L, -1)) {
    case LUA_TNIL:
        break;
    case LUA_TBOOLEAN:
        start_new_session = lua_toboolean(L, -1);
        break;
    default:
        push(L, std::errc::invalid_argument, "arg", "start_new_session");
        return lua_error(L);
    }
    lua_pop(L, 1);

    std::optional<pid_t> process_group;
    lua_getfield(L, 1, "process_group");
    switch (lua_type(L, -1)) {
    case LUA_TNIL:
        break;
    case LUA_TNUMBER:
        process_group.emplace(lua_tointeger(L, -1));
        break;
    default:
        push(L, std::errc::invalid_argument, "arg", "process_group");
        return lua_error(L);
    }
    lua_pop(L, 1);

    bool resetids = false;
    lua_getfield(L, 1, "resetids");
    switch (lua_type(L, -1)) {
    case LUA_TNIL:
        break;
    case LUA_TBOOLEAN:
        resetids = lua_toboolean(L, -1);
        break;
    default:
        push(L, std::errc::invalid_argument, "arg", "resetids");
        return lua_error(L);
    }
    lua_pop(L, 1);

    std::optional<std::string> chdir;
    lua_getfield(L, 1, "chdir");
    switch (lua_type(L, -1)) {
    case LUA_TNIL:
        break;
    case LUA_TSTRING:
        chdir.emplace(tostringview(L));
        break;
    default:
        push(L, std::errc::invalid_argument, "arg", "chdir");
        return lua_error(L);
    }
    lua_pop(L, 1);

    int pipefd[2];
    if (pipe2(pipefd, O_DIRECT) == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }
    BOOST_SCOPE_EXIT_ALL(&) {
        int res = close(pipefd[0]);
        boost::ignore_unused(res);
        if (pipefd[1] != -1) {
            res = close(pipefd[1]);
            boost::ignore_unused(res);
        }
    };

    spawn_arguments_t args;
    args.use_path = use_path;
    args.closeonexecpipe = pipefd[1];
    args.program = program.data();
    args.argv = argumentsb.data();
    args.envp = environmentb.data();
    args.proc_stdin = proc_stdin;
    args.proc_stdout = proc_stdout;
    args.proc_stderr = proc_stderr;
    args.signal_mask = signal_mask;
    args.signal_default_handlers = signal_default_handlers;
    args.scheduler_policy = scheduler_policy;
    args.scheduler_priority = scheduler_priority;
    args.start_new_session = start_new_session;
    args.process_group = process_group;
    args.resetids = resetids;
    args.chdir = chdir;

    int clone_flags = CLONE_PIDFD;
    int pidfd = -1;
    int childpid = clone(system_spawn_child_main, clone_stack_address,
                         clone_flags, &args, &pidfd);
    if (childpid == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }
    BOOST_SCOPE_EXIT_ALL(&) {
        if (pidfd != -1) {
            int res = kill(childpid, SIGKILL);
            boost::ignore_unused(res);
            siginfo_t info;
            res = waitid(P_PIDFD, pidfd, &info, WEXITED);
            boost::ignore_unused(res);
            res = close(pidfd);
            boost::ignore_unused(res);
        }
    };

    int res = close(pipefd[1]);
    boost::ignore_unused(res);
    pipefd[1] = -1;

    spawn_arguments_t::errno_reply_t reply;
    auto nread = read(pipefd[0], &reply, sizeof(reply));
    assert(nread != -1);
    if (nread != 0) {
        // exec() or pre-exec() failed
        push(L, std::error_code{reply.code, std::system_category()});
        return lua_error(L);
    }

    auto p = static_cast<subprocess*>(lua_newuserdata(L, sizeof(subprocess)));
    rawgetp(L, LUA_REGISTRYINDEX, &subprocess_mt_key);
    setmetatable(L, -2);
    new (p) subprocess{};

    p->reaper.emplace(
        vm_ctx.strand().context(), pidfd, childpid, signal_on_zombie);
    pidfd = -1;

    return 1;
}

static int system_spawn(lua_State* L)
{
    return system_spawn_do(/*use_path=*/false, L);
}

static int system_spawnp(lua_State* L)
{
    return system_spawn_do(/*use_path=*/true, L);
}
#endif // BOOST_OS_LINUX

static int system_mt_index(lua_State* L)
{
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("environment"), system_environment),
            hana::make_pair(BOOST_HANA_STRING("signal"), system_signal),
            hana::make_pair(BOOST_HANA_STRING("arguments"), system_arguments),
#if !BOOST_OS_WINDOWS || EMILUA_CONFIG_THREAD_SUPPORT_LEVEL >= 1
            hana::make_pair(BOOST_HANA_STRING("in_"), system_in),
#endif // !BOOST_OS_WINDOWS || EMILUA_CONFIG_THREAD_SUPPORT_LEVEL >= 1
            hana::make_pair(BOOST_HANA_STRING("out"), system_out),
            hana::make_pair(BOOST_HANA_STRING("err"), system_err),
#if BOOST_OS_LINUX
            hana::make_pair(
                BOOST_HANA_STRING("spawn"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, system_spawn);
                    return 1;
                }),
            hana::make_pair(
                BOOST_HANA_STRING("spawnp"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, system_spawnp);
                    return 1;
                }),
#endif // BOOST_OS_LINUX
#if BOOST_OS_UNIX
            hana::make_pair(
                BOOST_HANA_STRING("getresuid"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, system_getresuid);
                    return 1;
                }),
            hana::make_pair(
                BOOST_HANA_STRING("getresgid"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, system_getresgid);
                    return 1;
                }),
            hana::make_pair(
                BOOST_HANA_STRING("setresuid"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, system_setresuid);
                    return 1;
                }),
            hana::make_pair(
                BOOST_HANA_STRING("setresgid"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, system_setresgid);
                    return 1;
                }),
#endif // BOOST_OS_UNIX
            hana::make_pair(
                BOOST_HANA_STRING("exit"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, system_exit);
                    return 1;
                })
        ),
        [](std::string_view /*key*/, lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        },
        tostringview(L, 2),
        L
    );
}

void init_system(lua_State* L)
{
    lua_pushlightuserdata(L, &system_key);
    {
        lua_newuserdata(L, /*size=*/1);

        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/2);

            lua_pushliteral(L, "__metatable");
            lua_pushliteral(L, "system");
            lua_rawset(L, -3);

            lua_pushliteral(L, "__index");
            lua_pushcfunction(L, system_mt_index);
            lua_rawset(L, -3);
        }

        setmetatable(L, -2);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &system_signal_key);
    {
        lua_newtable(L);

        lua_pushliteral(L, "raise");
        lua_pushcfunction(L, system_signal_raise);
        lua_rawset(L, -3);

        lua_pushliteral(L, "set");
        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/1);

            lua_pushliteral(L, "new");
            lua_pushcfunction(L, system_signal_set_new);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);

#define EMILUA_DEF_SIGNAL(KEY, VALUE) do { \
            lua_pushliteral(L, KEY);       \
            lua_pushinteger(L, VALUE);     \
            lua_rawset(L, -3);             \
        } while(0)
#define EMILUA_DEF_SIGNAL2(SIG) EMILUA_DEF_SIGNAL(#SIG, SIG)

        // <signal.h>
        EMILUA_DEF_SIGNAL2(SIGABRT);
        EMILUA_DEF_SIGNAL2(SIGFPE);
        EMILUA_DEF_SIGNAL2(SIGILL);
        EMILUA_DEF_SIGNAL2(SIGINT);
        EMILUA_DEF_SIGNAL2(SIGSEGV);
        EMILUA_DEF_SIGNAL2(SIGTERM);

#define EMILUA_DEF_SIGNAL3(SIG) BOOST_PP_IIF( \
        BOOST_VMD_IS_NUMBER(SIG), EMILUA_DEF_SIGNAL, BOOST_VMD_EMPTY \
    )(#SIG, SIG)

        // Unix
        EMILUA_DEF_SIGNAL3(SIGALRM);
        EMILUA_DEF_SIGNAL3(SIGBUS);
        EMILUA_DEF_SIGNAL3(SIGCHLD);
        EMILUA_DEF_SIGNAL3(SIGCONT);
        EMILUA_DEF_SIGNAL3(SIGHUP);
        EMILUA_DEF_SIGNAL3(SIGIO);
        EMILUA_DEF_SIGNAL3(SIGKILL);
        EMILUA_DEF_SIGNAL3(SIGPIPE);
        EMILUA_DEF_SIGNAL3(SIGPROF);
        EMILUA_DEF_SIGNAL3(SIGQUIT);
        EMILUA_DEF_SIGNAL3(SIGSTOP);
        EMILUA_DEF_SIGNAL3(SIGSYS);
        EMILUA_DEF_SIGNAL3(SIGTRAP);
        EMILUA_DEF_SIGNAL3(SIGTSTP);
        EMILUA_DEF_SIGNAL3(SIGTTIN);
        EMILUA_DEF_SIGNAL3(SIGTTOU);
        EMILUA_DEF_SIGNAL3(SIGURG);
        EMILUA_DEF_SIGNAL3(SIGUSR1);
        EMILUA_DEF_SIGNAL3(SIGUSR2);
        EMILUA_DEF_SIGNAL3(SIGVTALRM);
        EMILUA_DEF_SIGNAL3(SIGWINCH);
        EMILUA_DEF_SIGNAL3(SIGXCPU);
        EMILUA_DEF_SIGNAL3(SIGXFSZ);

        // Windows
        EMILUA_DEF_SIGNAL3(SIGBREAK);

#undef EMILUA_DEF_SIGNAL3
#undef EMILUA_DEF_SIGNAL2
#undef EMILUA_DEF_SIGNAL
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

#if !BOOST_OS_WINDOWS || EMILUA_CONFIG_THREAD_SUPPORT_LEVEL >= 1
    lua_pushlightuserdata(L, &system_in_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/2);

        lua_pushliteral(L, "read_some");
        rawgetp(L, LUA_REGISTRYINDEX,
                &var_args__retval1_to_error__fwd_retval2__key);
        rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
        lua_pushcfunction(L, system_in_read_some);
        lua_call(L, 2, 1);
        lua_rawset(L, -3);

# if BOOST_OS_UNIX
        lua_pushliteral(L, "dup");
        lua_pushcfunction(L, system_stdhandle_dup<STDIN_FILENO>);
        lua_rawset(L, -3);
# endif // BOOST_OS_UNIX
    }
    lua_rawset(L, LUA_REGISTRYINDEX);
#endif // !BOOST_OS_WINDOWS || EMILUA_CONFIG_THREAD_SUPPORT_LEVEL >= 1

    lua_pushlightuserdata(L, &system_out_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/2);

        lua_pushliteral(L, "write_some");
#if BOOST_OS_WINDOWS
        lua_pushcfunction(L, system_out_write_some);
#else // BOOST_OS_WINDOWS
        rawgetp(L, LUA_REGISTRYINDEX,
                &var_args__retval1_to_error__fwd_retval2__key);
        rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
        lua_pushcfunction(L, system_out_write_some);
        lua_call(L, 2, 1);
#endif // BOOST_OS_WINDOWS
        lua_rawset(L, -3);

#if BOOST_OS_UNIX
        lua_pushliteral(L, "dup");
        lua_pushcfunction(L, system_stdhandle_dup<STDOUT_FILENO>);
        lua_rawset(L, -3);
#endif // BOOST_OS_UNIX
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &system_err_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/2);

        lua_pushliteral(L, "write_some");
#if BOOST_OS_WINDOWS
        lua_pushcfunction(L, system_err_write_some);
#else // BOOST_OS_WINDOWS
        rawgetp(L, LUA_REGISTRYINDEX,
                &var_args__retval1_to_error__fwd_retval2__key);
        rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
        lua_pushcfunction(L, system_err_write_some);
        lua_call(L, 2, 1);
#endif // BOOST_OS_WINDOWS
        lua_rawset(L, -3);

#if BOOST_OS_UNIX
        lua_pushliteral(L, "dup");
        lua_pushcfunction(L, system_stdhandle_dup<STDERR_FILENO>);
        lua_rawset(L, -3);
#endif // BOOST_OS_UNIX
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &system_signal_set_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "system.signal.set");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, system_signal_set_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<asio::signal_set>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &system_signal_set_wait_key);
    rawgetp(L, LUA_REGISTRYINDEX,
            &var_args__retval1_to_error__fwd_retval2__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, system_signal_set_wait);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

#if BOOST_OS_LINUX
    lua_pushlightuserdata(L, &subprocess_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "subprocess");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, subprocess_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<subprocess>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &subprocess_wait_key);
    rawgetp(L, LUA_REGISTRYINDEX, &var_args__retval1_to_error__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, subprocess_wait);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);
#endif // BOOST_OS_LINUX
}

} // namespace emilua
