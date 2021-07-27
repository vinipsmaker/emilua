/* Copyright (c) 2021 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#include <emilua/sys.hpp>

#include <csignal>

#include <boost/preprocessor/control/iif.hpp>
#include <boost/process/environment.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/vmd/is_number.hpp>
#include <boost/vmd/empty.hpp>

#include <emilua/dispatch_table.hpp>
#include <emilua/fiber.hpp>

namespace emilua {

extern unsigned char signal_set_wait_bytecode[];
extern std::size_t signal_set_wait_bytecode_size;

char sys_key;

static char sys_signal_key;
static char sys_signal_set_mt_key;
static char sys_signal_set_wait_key;

static int sys_signal_set_new(lua_State* L)
{
    int nargs = lua_gettop(L);

    auto& vm_ctx = get_vm_context(L);
    if (vm_ctx.appctx.main_vm.lock().get() != &vm_ctx) {
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
            std::error_code std_ec = ec;
            vm_ctx->fiber_prologue(
                current_fiber,
                [&]() {
                    if (ec == asio::error::operation_aborted) {
                        rawgetp(current_fiber, LUA_REGISTRYINDEX,
                                &fiber_list_key);
                        lua_pushthread(current_fiber);
                        lua_rawget(current_fiber, -2);
                        lua_rawgeti(current_fiber, -1,
                                    FiberDataIndex::INTERRUPTED);
                        bool interrupted = lua_toboolean(current_fiber, -1);
                        lua_pop(current_fiber, 3);
                        if (interrupted)
                            std_ec = errc::interrupted;
                    }
                    push(current_fiber, std_ec);
                    lua_pushinteger(current_fiber, signal_number);
                });
            int res = lua_resume(current_fiber, 2);
            vm_ctx->fiber_epilogue(res);
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

    // SIGKILL and SIGSTOP are the only signals that cannot be caught, blocked,
    // or ignored. If we allowed any child VM to raise these signals, then the
    // protection to only allow the main VM to force-exit the process would be
    // moot.
    if (
        EMILUA_IS_SIG_X_OR(SIGKILL)
        EMILUA_IS_SIG_X_OR(SIGSTOP)
        false
    ) {
        auto& vm_ctx = get_vm_context(L);
        if (vm_ctx.appctx.main_vm.lock().get() != &vm_ctx) {
            push(L, std::errc::operation_not_permitted);
            return lua_error(L);
        }
    }

    // Unless the main VM has a handler installed (the check doesn't need to be
    // race-free... that's not a problem) for these signals, forbid slave VMs
    // from raising them.
    //
    // Also I don't really like this blacklist-based implementation. A
    // whitelist-based implementation would be safer. However I'll allow the
    // blacklist approach just this time because we don't see new signals
    // popping up everyday.
    if (
        // Default action is to terminate the process
        EMILUA_IS_SIG_X_OR(SIGALRM)
        EMILUA_IS_SIG_X_OR(SIGEMT)
        EMILUA_IS_SIG_X_OR(SIGHUP)
        EMILUA_IS_SIG_X_OR(SIGINT)
        EMILUA_IS_SIG_X_OR(SIGIO)
        EMILUA_IS_SIG_X_OR(SIGLOST)
        EMILUA_IS_SIG_X_OR(SIGPIPE)
        EMILUA_IS_SIG_X_OR(SIGPOLL)
        EMILUA_IS_SIG_X_OR(SIGPROF)
        EMILUA_IS_SIG_X_OR(SIGPWR)
        EMILUA_IS_SIG_X_OR(SIGSTKFLT)
        EMILUA_IS_SIG_X_OR(SIGTERM)
        EMILUA_IS_SIG_X_OR(SIGUSR1)
        EMILUA_IS_SIG_X_OR(SIGUSR2)
        EMILUA_IS_SIG_X_OR(SIGVTALRM)

        // Default action is to terminate the process and dump core
        EMILUA_IS_SIG_X_OR(SIGABRT)
        EMILUA_IS_SIG_X_OR(SIGBUS)
        EMILUA_IS_SIG_X_OR(SIGFPE)
        EMILUA_IS_SIG_X_OR(SIGILL)
        EMILUA_IS_SIG_X_OR(SIGIOT)
        EMILUA_IS_SIG_X_OR(SIGQUIT)
        EMILUA_IS_SIG_X_OR(SIGSEGV)
        EMILUA_IS_SIG_X_OR(SIGSYS)
        EMILUA_IS_SIG_X_OR(SIGTRAP)
        EMILUA_IS_SIG_X_OR(SIGUNUSED)
        EMILUA_IS_SIG_X_OR(SIGXCPU)
        EMILUA_IS_SIG_X_OR(SIGXFSZ)

        // Default action is to stop the process
        EMILUA_IS_SIG_X_OR(SIGTSTP)
        EMILUA_IS_SIG_X_OR(SIGTTIN)
        EMILUA_IS_SIG_X_OR(SIGTTOU)

        false
    ) {
#ifdef _POSIX_C_SOURCE
        auto& vm_ctx = get_vm_context(L);
        if (vm_ctx.appctx.main_vm.lock().get() != &vm_ctx) {
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
        }
#endif // _POSIX_C_SOURCE
    }

#undef EMILUA_IS_SIG_X_OR
#undef EMILUA_DETAIL_IS_SIG_X_OR

    int ret = std::raise(signal_number);
    if (ret != 0) {
        push(L, errc::raise_error, "ret", ret);
        return lua_error(L);
    }
    return 0;
}

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
    if (vm_ctx.appctx.main_vm.lock().get() == &vm_ctx) {
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
        EMILUA_DEF_SIGNAL3(SIGKILL);
        EMILUA_DEF_SIGNAL3(SIGPIPE);
        EMILUA_DEF_SIGNAL3(SIGTRAP);
        EMILUA_DEF_SIGNAL3(SIGUSR1);
        EMILUA_DEF_SIGNAL3(SIGUSR2);
        EMILUA_DEF_SIGNAL3(SIGWINCH);

        // Windows
        EMILUA_DEF_SIGNAL3(SIGBREAK);

#undef EMILUA_DEF_SIGNAL3
#undef EMILUA_DEF_SIGNAL2
#undef EMILUA_DEF_SIGNAL
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
