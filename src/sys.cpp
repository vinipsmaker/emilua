/* Copyright (c) 2021, 2022 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#include <emilua/sys.hpp>
#include <emilua/byte_span.hpp>

#include <csignal>
#include <cstdlib>

#include <boost/preprocessor/control/iif.hpp>
#include <boost/process/environment.hpp>
#include <boost/predef/os/windows.h>
#include <boost/asio/signal_set.hpp>
#include <boost/vmd/is_number.hpp>
#include <boost/vmd/empty.hpp>

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

namespace emilua {

#if !BOOST_OS_WINDOWS || EMILUA_CONFIG_THREAD_SUPPORT_LEVEL >= 1
// from bytecode/ip.lua
extern unsigned char data_op_bytecode[];
extern std::size_t data_op_bytecode_size;
#endif // !BOOST_OS_WINDOWS || EMILUA_CONFIG_THREAD_SUPPORT_LEVEL >= 1

extern unsigned char signal_set_wait_bytecode[];
extern std::size_t signal_set_wait_bytecode_size;

char sys_key;

static char sys_signal_key;
static char sys_signal_set_mt_key;
static char sys_signal_set_wait_key;

#if !BOOST_OS_WINDOWS || EMILUA_CONFIG_THREAD_SUPPORT_LEVEL >= 1
static char sys_stdin_key;
#endif // !BOOST_OS_WINDOWS || EMILUA_CONFIG_THREAD_SUPPORT_LEVEL >= 1
static char sys_stdout_key;
static char sys_stderr_key;

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
static int sys_stdout_write_some(lua_State* L)
{
    // we don't really need to check for suspend-allowed here given no
    // fiber-switch happens and we block the whole thread, but it's better to do
    // it anyway to guarantee the function will behave the same across different
    // platforms
    auto& vm_ctx = get_vm_context(L);
    EMILUA_CHECK_SUSPEND_ALLOWED(vm_ctx, L);

    auto bs = reinterpret_cast<byte_span_handle*>(lua_touserdata(L, 2));
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

static int sys_stderr_write_some(lua_State* L)
{
    // we don't really need to check for suspend-allowed here given no
    // fiber-switch happens and we block the whole thread, but it's better to do
    // it anyway to guarantee the function will behave the same across different
    // platforms
    auto& vm_ctx = get_vm_context(L);
    EMILUA_CHECK_SUSPEND_ALLOWED(vm_ctx, L);

    auto bs = reinterpret_cast<byte_span_handle*>(lua_touserdata(L, 2));
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

static int sys_signal_set_new(lua_State* L)
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

    auto set = reinterpret_cast<asio::signal_set*>(
        lua_newuserdata(L, sizeof(asio::signal_set))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &sys_signal_set_mt_key);
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

static int sys_signal_set_wait(lua_State* L)
{
    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto set = reinterpret_cast<asio::signal_set*>(lua_touserdata(L, 1));
    if (!set || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &sys_signal_set_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    lua_pushvalue(L, 1);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto set = reinterpret_cast<asio::signal_set*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            boost::system::error_code ignored_ec;
            set->cancel(ignored_ec);
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    set->async_wait(asio::bind_executor(
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
    ));

    return lua_yield(L, 0);
}

static int sys_signal_set_add(lua_State* L)
{
    lua_settop(L, 2);

    auto set = reinterpret_cast<asio::signal_set*>(lua_touserdata(L, 1));
    if (!set || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &sys_signal_set_mt_key);
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

static int sys_signal_set_remove(lua_State* L)
{
    lua_settop(L, 2);

    auto set = reinterpret_cast<asio::signal_set*>(lua_touserdata(L, 1));
    if (!set || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &sys_signal_set_mt_key);
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

static int sys_signal_set_clear(lua_State* L)
{
    auto set = reinterpret_cast<asio::signal_set*>(lua_touserdata(L, 1));
    if (!set || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &sys_signal_set_mt_key);
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

static int sys_signal_set_cancel(lua_State* L)
{
    auto set = reinterpret_cast<asio::signal_set*>(lua_touserdata(L, 1));
    if (!set || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &sys_signal_set_mt_key);
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

static int sys_signal_set_mt_index(lua_State* L)
{
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("wait"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX, &sys_signal_set_wait_key);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("cancel"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, sys_signal_set_cancel);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("add"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, sys_signal_set_add);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("remove"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, sys_signal_set_remove);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("clear"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, sys_signal_set_clear);
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

static int sys_signal_raise(lua_State* L)
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
static int sys_stdin_read_some(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);
    EMILUA_CHECK_SUSPEND_ALLOWED(vm_ctx, L);

    auto bs = reinterpret_cast<byte_span_handle*>(lua_touserdata(L, 2));
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
            auto service = reinterpret_cast<stdin_service*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            auto fiber = reinterpret_cast<lua_State*>(
                lua_touserdata(L, lua_upvalueindex(2)));

            auto vm_ctx = get_vm_context(L).shared_from_this();

            {
                std::unique_lock<std::mutex> lk(service->queue_mtx);
                auto it = service->queue.begin();
                auto end = service->queue.end();
                for (; it != end ; ++it) {
                    if (it->fiber == fiber) {
                        service->queue.erase(it);
                        vm_ctx->strand().defer(
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
static int sys_stdin_read_some(lua_State* L)
{
    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto bs = reinterpret_cast<byte_span_handle*>(lua_touserdata(L, 2));
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

static int sys_stdout_write_some(lua_State* L)
{
    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto bs = reinterpret_cast<byte_span_handle*>(lua_touserdata(L, 2));
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

static int sys_stderr_write_some(lua_State* L)
{
    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto bs = reinterpret_cast<byte_span_handle*>(lua_touserdata(L, 2));
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

inline int sys_args(lua_State* L)
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

inline int sys_env(lua_State* L)
{
    auto env = boost::this_process::environment();
    lua_createtable(L, /*narr=*/0, /*nrec=*/env.size());
    for (const auto& e: env) {
        push(L, e.get_name());
        push(L, e.to_string());
        lua_rawset(L, -3);
    }
    return 1;
}

#if !BOOST_OS_WINDOWS || EMILUA_CONFIG_THREAD_SUPPORT_LEVEL >= 1
inline int sys_stdin(lua_State* L)
{
    rawgetp(L, LUA_REGISTRYINDEX, &sys_stdin_key);
    return 1;
}
#endif // !BOOST_OS_WINDOWS || EMILUA_CONFIG_THREAD_SUPPORT_LEVEL >= 1

inline int sys_stdout(lua_State* L)
{
    rawgetp(L, LUA_REGISTRYINDEX, &sys_stdout_key);
    return 1;
}

inline int sys_stderr(lua_State* L)
{
    rawgetp(L, LUA_REGISTRYINDEX, &sys_stderr_key);
    return 1;
}

inline int sys_signal(lua_State* L)
{
    rawgetp(L, LUA_REGISTRYINDEX, &sys_signal_key);
    return 1;
}

static int sys_exit(lua_State* L)
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
                    std::quick_exit(exit_code);
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

static int sys_mt_index(lua_State* L)
{
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(BOOST_HANA_STRING("env"), sys_env),
            hana::make_pair(BOOST_HANA_STRING("signal"), sys_signal),
            hana::make_pair(BOOST_HANA_STRING("args"), sys_args),
#if !BOOST_OS_WINDOWS || EMILUA_CONFIG_THREAD_SUPPORT_LEVEL >= 1
            hana::make_pair(BOOST_HANA_STRING("stdin"), sys_stdin),
#endif // !BOOST_OS_WINDOWS || EMILUA_CONFIG_THREAD_SUPPORT_LEVEL >= 1
            hana::make_pair(BOOST_HANA_STRING("stdout"), sys_stdout),
            hana::make_pair(BOOST_HANA_STRING("stderr"), sys_stderr),
            hana::make_pair(
                BOOST_HANA_STRING("exit"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, sys_exit);
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

void init_sys(lua_State* L)
{
    lua_pushlightuserdata(L, &sys_key);
    {
        lua_newuserdata(L, /*size=*/1);

        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/2);

            lua_pushliteral(L, "__metatable");
            lua_pushliteral(L, "sys");
            lua_rawset(L, -3);

            lua_pushliteral(L, "__index");
            lua_pushcfunction(L, sys_mt_index);
            lua_rawset(L, -3);
        }

        setmetatable(L, -2);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &sys_signal_key);
    {
        lua_newtable(L);

        lua_pushliteral(L, "raise");
        lua_pushcfunction(L, sys_signal_raise);
        lua_rawset(L, -3);

        lua_pushliteral(L, "set");
        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/1);

            lua_pushliteral(L, "new");
            lua_pushcfunction(L, sys_signal_set_new);
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
    lua_pushlightuserdata(L, &sys_stdin_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/1);

        lua_pushliteral(L, "read_some");
        int res = luaL_loadbuffer(L, reinterpret_cast<char*>(data_op_bytecode),
                                  data_op_bytecode_size, nullptr);
        assert(res == 0); boost::ignore_unused(res);
        rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
        lua_pushcfunction(L, sys_stdin_read_some);
        lua_call(L, 2, 1);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);
#endif // !BOOST_OS_WINDOWS || EMILUA_CONFIG_THREAD_SUPPORT_LEVEL >= 1

    lua_pushlightuserdata(L, &sys_stdout_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/1);

        lua_pushliteral(L, "write_some");
#if BOOST_OS_WINDOWS
        lua_pushcfunction(L, sys_stdout_write_some);
#else // BOOST_OS_WINDOWS
        int res = luaL_loadbuffer(L, reinterpret_cast<char*>(data_op_bytecode),
                                  data_op_bytecode_size, nullptr);
        assert(res == 0); boost::ignore_unused(res);
        rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
        lua_pushcfunction(L, sys_stdout_write_some);
        lua_call(L, 2, 1);
#endif // BOOST_OS_WINDOWS
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &sys_stderr_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/1);

        lua_pushliteral(L, "write_some");
#if BOOST_OS_WINDOWS
        lua_pushcfunction(L, sys_stderr_write_some);
#else // BOOST_OS_WINDOWS
        int res = luaL_loadbuffer(L, reinterpret_cast<char*>(data_op_bytecode),
                                  data_op_bytecode_size, nullptr);
        assert(res == 0); boost::ignore_unused(res);
        rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
        lua_pushcfunction(L, sys_stderr_write_some);
        lua_call(L, 2, 1);
#endif // BOOST_OS_WINDOWS
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &sys_signal_set_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "sys.signal.set");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, sys_signal_set_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<asio::signal_set>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &sys_signal_set_wait_key);
    int res = luaL_loadbuffer(L,
                              reinterpret_cast<char*>(signal_set_wait_bytecode),
                              signal_set_wait_bytecode_size, nullptr);
    assert(res == 0); boost::ignore_unused(res);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, sys_signal_set_wait);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);
}

} // namespace emilua
