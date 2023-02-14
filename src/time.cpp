/* Copyright (c) 2020, 2021, 2023 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

// workaround for Boost.Asio bug
#if defined(BOOST_ASIO_HAS_IO_URING) && defined(BOOST_ASIO_DISABLE_EPOLL)
#include <boost/asio/detail/scheduler.hpp>
#endif // defined(BOOST_ASIO_HAS_IO_URING) && defined(BOOST_ASIO_DISABLE_EPOLL)

#include <boost/asio/steady_timer.hpp>

#include <emilua/dispatch_table.hpp>
#include <emilua/async_base.hpp>
#include <emilua/time.hpp>

namespace emilua {

char time_key;
char system_clock_time_point_mt_key;
static char system_timer_mt_key;
static char system_timer_wait_key;
static char steady_clock_time_point_mt_key;
static char high_resolution_clock_time_point_mt_key;
static char steady_timer_mt_key;
static char steady_timer_wait_key;

using lua_Seconds = std::chrono::duration<lua_Number>;

struct sleep_for_operation: public pending_operation
{
    sleep_for_operation(asio::io_context& ctx)
        : pending_operation{/*shared_ownership=*/true}
        , timer{ctx}
    {}

    void cancel() noexcept override
    {
        try {
            timer.cancel();
        } catch (const boost::system::system_error&) {}
    }

    asio::steady_timer timer;
};

template<class Clock>
struct handle_type
{
    handle_type(asio::io_context& ctx)
        : timer{ctx}
    {}

    asio::basic_waitable_timer<Clock> timer;
};

static int sleep_for(lua_State* L)
{
    lua_Number secs = luaL_checknumber(L, 1);
    if (std::isnan(secs) || std::isinf(secs) || secs < 0) {
        push(L, std::errc::argument_out_of_domain, "arg", 1);
        return lua_error(L);
    }

    lua_Seconds dur{secs};
    if (dur > asio::steady_timer::duration::max()) {
        push(L, std::errc::value_too_large);
        return lua_error(L);
    }

    auto dur2 = std::chrono::ceil<asio::steady_timer::duration>(dur);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto handle = std::make_shared<sleep_for_operation>(
        vm_ctx->strand().context());
    handle->timer.expires_after(dur2);

    lua_pushlightuserdata(L, handle.get());
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto handle = static_cast<sleep_for_operation*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            try {
                handle->timer.cancel();
            } catch (const boost::system::system_error&) {}
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    vm_ctx->pending_operations.push_back(*handle);

    handle->timer.async_wait(asio::bind_executor(
        vm_ctx->strand_using_defer(),
        [vm_ctx,current_fiber,handle](const boost::system::error_code &ec) {
            if (vm_ctx->valid()) {
                vm_ctx->pending_operations.erase(
                    vm_ctx->pending_operations.iterator_to(*handle));
            }
            auto opt_args = vm_context::options::arguments;
            vm_ctx->fiber_resume(
                current_fiber,
                hana::make_set(
                    vm_context::options::fast_auto_detect_interrupt,
                    hana::make_pair(opt_args, hana::make_tuple(ec))));
        }
    ));

    return lua_yield(L, 0);
}

static int steady_clock_time_point_add(lua_State* L)
{
    lua_settop(L, 2);

    auto tp = static_cast<std::chrono::steady_clock::time_point*>(
        lua_touserdata(L, 1));
    if (!tp || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &steady_clock_time_point_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    lua_Number secs = luaL_checknumber(L, 2);
    if (std::isnan(secs) || std::isinf(secs)) {
        push(L, std::errc::argument_out_of_domain, "arg", 2);
        return lua_error(L);
    }

    lua_Seconds dur{secs};
    if (
        dur > std::chrono::steady_clock::duration::max() ||
        dur < std::chrono::steady_clock::duration::min()
    ) {
        push(L, std::errc::value_too_large);
        return lua_error(L);
    }

    try {
        *tp += std::chrono::round<std::chrono::steady_clock::duration>(dur);
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }
    return 0;
}

static int steady_clock_time_point_sub(lua_State* L)
{
    lua_settop(L, 2);

    auto tp = static_cast<std::chrono::steady_clock::time_point*>(
        lua_touserdata(L, 1));
    if (!tp || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &steady_clock_time_point_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    lua_Number secs = luaL_checknumber(L, 2);
    if (std::isnan(secs) || std::isinf(secs)) {
        push(L, std::errc::argument_out_of_domain, "arg", 2);
        return lua_error(L);
    }

    lua_Seconds dur{secs};
    if (
        dur > std::chrono::steady_clock::duration::max() ||
        dur < std::chrono::steady_clock::duration::min()
    ) {
        push(L, std::errc::value_too_large);
        return lua_error(L);
    }

    try {
        *tp -= std::chrono::round<std::chrono::steady_clock::duration>(dur);
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }
    return 0;
}

inline int steady_clock_time_point_seconds_since_epoch(lua_State* L)
{
    auto tp = static_cast<std::chrono::steady_clock::time_point*>(
        lua_touserdata(L, 1));
    lua_pushnumber(L, lua_Seconds{tp->time_since_epoch()}.count());
    return 1;
}

static int steady_clock_time_point_mt_index(lua_State* L)
{
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("add"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, steady_clock_time_point_add);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("sub"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, steady_clock_time_point_sub);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("seconds_since_epoch"),
                steady_clock_time_point_seconds_since_epoch
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

static int steady_clock_time_point_mt_eq(lua_State* L)
{
    auto tp1 = static_cast<std::chrono::steady_clock::time_point*>(
        lua_touserdata(L, 1));
    auto tp2 = static_cast<std::chrono::steady_clock::time_point*>(
        lua_touserdata(L, 2));
    lua_pushboolean(L, *tp1 == *tp2);
    return 1;
}

static int steady_clock_time_point_mt_lt(lua_State* L)
{
    auto tp1 = static_cast<std::chrono::steady_clock::time_point*>(
        lua_touserdata(L, 1));
    if (!tp1 || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &steady_clock_time_point_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto tp2 = static_cast<std::chrono::steady_clock::time_point*>(
        lua_touserdata(L, 2));
    if (!tp2 || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &steady_clock_time_point_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    lua_pushboolean(L, *tp1 < *tp2);
    return 1;
}

static int steady_clock_time_point_mt_le(lua_State* L)
{
    auto tp1 = static_cast<std::chrono::steady_clock::time_point*>(
        lua_touserdata(L, 1));
    if (!tp1 || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &steady_clock_time_point_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto tp2 = static_cast<std::chrono::steady_clock::time_point*>(
        lua_touserdata(L, 2));
    if (!tp2 || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &steady_clock_time_point_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    lua_pushboolean(L, *tp1 <= *tp2);
    return 1;
}

static int steady_clock_time_point_mt_add(lua_State* L)
{
    auto tp = static_cast<std::chrono::steady_clock::time_point*>(
        lua_touserdata(L, 1));
    if (!tp || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &steady_clock_time_point_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    lua_Number secs = luaL_checknumber(L, 2);
    if (std::isnan(secs) || std::isinf(secs)) {
        push(L, std::errc::argument_out_of_domain, "arg", 2);
        return lua_error(L);
    }

    lua_Seconds dur{secs};
    if (
        dur > std::chrono::steady_clock::duration::max() ||
        dur < std::chrono::steady_clock::duration::min()
    ) {
        push(L, std::errc::value_too_large);
        return lua_error(L);
    }

    auto ret = static_cast<std::chrono::steady_clock::time_point*>(
        lua_newuserdata(L, sizeof(std::chrono::steady_clock::time_point))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &steady_clock_time_point_mt_key);
    setmetatable(L, -2);
    new (ret) std::chrono::steady_clock::time_point{};

    try {
        *ret = *tp +
            std::chrono::round<std::chrono::steady_clock::duration>(dur);
        return 1;
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }
}

static int steady_clock_time_point_mt_sub(lua_State* L)
{
    auto tp = static_cast<std::chrono::steady_clock::time_point*>(
        lua_touserdata(L, 1));
    if (!tp || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &steady_clock_time_point_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    switch (lua_type(L, 2)) {
    case LUA_TNUMBER: {
        lua_Number secs = lua_tonumber(L, 2);
        if (std::isnan(secs) || std::isinf(secs)) {
            push(L, std::errc::argument_out_of_domain, "arg", 2);
            return lua_error(L);
        }

        lua_Seconds dur{secs};
        if (
            dur > std::chrono::steady_clock::duration::max() ||
            dur < std::chrono::steady_clock::duration::min()
        ) {
            push(L, std::errc::value_too_large);
            return lua_error(L);
        }

        auto ret = static_cast<std::chrono::steady_clock::time_point*>(
            lua_newuserdata(L, sizeof(std::chrono::steady_clock::time_point))
        );
        rawgetp(L, LUA_REGISTRYINDEX, &steady_clock_time_point_mt_key);
        setmetatable(L, -2);
        new (ret) std::chrono::steady_clock::time_point{};

        try {
            *ret = *tp -
                std::chrono::round<std::chrono::steady_clock::duration>(dur);
            return 1;
        } catch (const std::system_error& e) {
            push(L, e.code());
            return lua_error(L);
        } catch (const std::exception& e) {
            lua_pushstring(L, e.what());
            return lua_error(L);
        }
    }
    case LUA_TUSERDATA: {
        auto tp2 = static_cast<std::chrono::steady_clock::time_point*>(
            lua_touserdata(L, 2));
        if (!tp2 || !lua_getmetatable(L, 2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &steady_clock_time_point_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        try {
            lua_pushnumber(L, lua_Seconds{*tp - *tp2}.count());
            return 1;
        } catch (const std::system_error& e) {
            push(L, e.code());
            return lua_error(L);
        } catch (const std::exception& e) {
            lua_pushstring(L, e.what());
            return lua_error(L);
        }
    }
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
}

static int steady_clock_epoch(lua_State* L)
{
    auto tp = static_cast<std::chrono::steady_clock::time_point*>(
        lua_newuserdata(L, sizeof(std::chrono::steady_clock::time_point))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &steady_clock_time_point_mt_key);
    setmetatable(L, -2);
    new (tp) std::chrono::steady_clock::time_point{};
    return 1;
}

static int steady_clock_now(lua_State* L)
{
    auto tp = static_cast<std::chrono::steady_clock::time_point*>(
        lua_newuserdata(L, sizeof(std::chrono::steady_clock::time_point))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &steady_clock_time_point_mt_key);
    setmetatable(L, -2);
    new (tp) std::chrono::steady_clock::time_point{};
    *tp = std::chrono::steady_clock::now();
    return 1;
}

inline int high_resolution_clock_time_point_seconds_since_epoch(lua_State* L)
{
    auto tp = static_cast<std::chrono::high_resolution_clock::time_point*>(
        lua_touserdata(L, 1));
    lua_pushnumber(L, lua_Seconds{tp->time_since_epoch()}.count());
    return 1;
}

static int high_resolution_clock_time_point_mt_index(lua_State* L)
{
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("seconds_since_epoch"),
                high_resolution_clock_time_point_seconds_since_epoch
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

static int high_resolution_clock_time_point_mt_eq(lua_State* L)
{
    auto tp1 = static_cast<std::chrono::high_resolution_clock::time_point*>(
        lua_touserdata(L, 1));
    auto tp2 = static_cast<std::chrono::high_resolution_clock::time_point*>(
        lua_touserdata(L, 2));
    lua_pushboolean(L, *tp1 == *tp2);
    return 1;
}

static int high_resolution_clock_time_point_mt_lt(lua_State* L)
{
    auto tp1 = static_cast<std::chrono::high_resolution_clock::time_point*>(
        lua_touserdata(L, 1));
    if (!tp1 || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &high_resolution_clock_time_point_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto tp2 = static_cast<std::chrono::high_resolution_clock::time_point*>(
        lua_touserdata(L, 2));
    if (!tp2 || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &high_resolution_clock_time_point_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    lua_pushboolean(L, *tp1 < *tp2);
    return 1;
}

static int high_resolution_clock_time_point_mt_le(lua_State* L)
{
    auto tp1 = static_cast<std::chrono::high_resolution_clock::time_point*>(
        lua_touserdata(L, 1));
    if (!tp1 || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &high_resolution_clock_time_point_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto tp2 = static_cast<std::chrono::high_resolution_clock::time_point*>(
        lua_touserdata(L, 2));
    if (!tp2 || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &high_resolution_clock_time_point_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    lua_pushboolean(L, *tp1 <= *tp2);
    return 1;
}

static int high_resolution_clock_time_point_mt_sub(lua_State* L)
{
    auto tp1 = static_cast<std::chrono::high_resolution_clock::time_point*>(
        lua_touserdata(L, 1));
    if (!tp1 || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &high_resolution_clock_time_point_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto tp2 = static_cast<std::chrono::high_resolution_clock::time_point*>(
        lua_touserdata(L, 2));
    if (!tp2 || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &high_resolution_clock_time_point_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    try {
        lua_pushnumber(L, lua_Seconds{*tp1 - *tp2}.count());
        return 1;
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }
}

static int high_resolution_clock_epoch(lua_State* L)
{
    auto tp = static_cast<std::chrono::high_resolution_clock::time_point*>(
        lua_newuserdata(
            L, sizeof(std::chrono::high_resolution_clock::time_point)
        )
    );
    rawgetp(L, LUA_REGISTRYINDEX, &high_resolution_clock_time_point_mt_key);
    setmetatable(L, -2);
    new (tp) std::chrono::high_resolution_clock::time_point{};
    return 1;
}

static int high_resolution_clock_now(lua_State* L)
{
    auto tp = static_cast<std::chrono::high_resolution_clock::time_point*>(
        lua_newuserdata(
            L, sizeof(std::chrono::high_resolution_clock::time_point)
        )
    );
    rawgetp(L, LUA_REGISTRYINDEX, &high_resolution_clock_time_point_mt_key);
    setmetatable(L, -2);
    new (tp) std::chrono::high_resolution_clock::time_point{};
    *tp = std::chrono::high_resolution_clock::now();
    return 1;
}

static int steady_timer_wait(lua_State* L)
{
    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto handle = static_cast<handle_type<std::chrono::steady_clock>*>(
        lua_touserdata(L, 1));
    if (!handle || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &steady_timer_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto cancel_slot = set_default_interrupter(L, *vm_ctx);

    handle->timer.async_wait(
        asio::bind_cancellation_slot(cancel_slot, asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber](const boost::system::error_code& ec) {
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

static int steady_timer_expires_at(lua_State* L)
{
    auto handle = static_cast<handle_type<std::chrono::steady_clock>*>(
        lua_touserdata(L, 1));
    if (!handle || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &steady_timer_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto tp = static_cast<std::chrono::steady_clock::time_point*>(
        lua_touserdata(L, 2));
    if (!tp || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &steady_clock_time_point_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    try {
        auto n = handle->timer.expires_at(*tp);
        lua_pushinteger(L, n);
        return 1;
    } catch (const boost::system::system_error& e) {
        push(L, static_cast<std::error_code>(e.code()));
        return lua_error(L);
    }
}

static int steady_timer_expires_after(lua_State* L)
{
    auto handle = static_cast<handle_type<std::chrono::steady_clock>*>(
        lua_touserdata(L, 1));
    if (!handle || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &steady_timer_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    lua_Number secs = luaL_checknumber(L, 2);
    if (std::isnan(secs) || std::isinf(secs) || secs < 0) {
        push(L, std::errc::argument_out_of_domain, "arg", 2);
        return lua_error(L);
    }

    lua_Seconds dur{secs};
    if (dur > asio::steady_timer::duration::max()) {
        push(L, std::errc::value_too_large);
        return lua_error(L);
    }

    auto dur2 = std::chrono::ceil<asio::steady_timer::duration>(dur);

    try {
        auto n = handle->timer.expires_after(dur2);
        lua_pushinteger(L, n);
        return 1;
    } catch (const boost::system::system_error& e) {
        push(L, static_cast<std::error_code>(e.code()));
        return lua_error(L);
    }
}

static int steady_timer_cancel(lua_State* L)
{
    auto handle = static_cast<handle_type<std::chrono::steady_clock>*>(
        lua_touserdata(L, 1));
    if (!handle || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &steady_timer_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    try {
        auto n = handle->timer.cancel();
        lua_pushinteger(L, n);
        return 1;
    } catch (const boost::system::system_error& e) {
        push(L, static_cast<std::error_code>(e.code()));
        return lua_error(L);
    }
}

inline int steady_timer_expiry(lua_State* L)
{
    auto handle = static_cast<handle_type<std::chrono::steady_clock>*>(
        lua_touserdata(L, 1));

    auto tp = static_cast<std::chrono::steady_clock::time_point*>(
        lua_newuserdata(L, sizeof(std::chrono::steady_clock::time_point))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &steady_clock_time_point_mt_key);
    setmetatable(L, -2);
    new (tp) std::chrono::steady_clock::time_point{};
    *tp = handle->timer.expiry();
    return 1;
}

static int steady_timer_mt_index(lua_State* L)
{
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("wait"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX, &steady_timer_wait_key);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("expires_at"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, steady_timer_expires_at);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("expires_after"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, steady_timer_expires_after);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("cancel"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, steady_timer_cancel);
                    return 1;
                }
            ),
            hana::make_pair(BOOST_HANA_STRING("expiry"), steady_timer_expiry)
        ),
        [](std::string_view /*key*/, lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        },
        tostringview(L, 2),
        L
    );
}

static int steady_timer_new(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);
    auto buf = static_cast<handle_type<std::chrono::steady_clock>*>(
        lua_newuserdata(L, sizeof(handle_type<std::chrono::steady_clock>))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &steady_timer_mt_key);
    setmetatable(L, -2);
    new (buf) handle_type<std::chrono::steady_clock>{vm_ctx.strand().context()};
    return 1;
}

static int system_clock_time_point_add(lua_State* L)
{
    lua_settop(L, 2);

    auto tp = static_cast<std::chrono::system_clock::time_point*>(
        lua_touserdata(L, 1));
    if (!tp || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &system_clock_time_point_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    lua_Number secs = luaL_checknumber(L, 2);
    if (std::isnan(secs) || std::isinf(secs)) {
        push(L, std::errc::argument_out_of_domain, "arg", 2);
        return lua_error(L);
    }

    lua_Seconds dur{secs};
    if (
        dur > std::chrono::system_clock::duration::max() ||
        dur < std::chrono::system_clock::duration::min()
    ) {
        push(L, std::errc::value_too_large);
        return lua_error(L);
    }

    try {
        *tp += std::chrono::round<std::chrono::system_clock::duration>(dur);
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }
    return 0;
}

static int system_clock_time_point_sub(lua_State* L)
{
    lua_settop(L, 2);

    auto tp = static_cast<std::chrono::system_clock::time_point*>(
        lua_touserdata(L, 1));
    if (!tp || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &system_clock_time_point_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    lua_Number secs = luaL_checknumber(L, 2);
    if (std::isnan(secs) || std::isinf(secs)) {
        push(L, std::errc::argument_out_of_domain, "arg", 2);
        return lua_error(L);
    }

    lua_Seconds dur{secs};
    if (
        dur > std::chrono::system_clock::duration::max() ||
        dur < std::chrono::system_clock::duration::min()
    ) {
        push(L, std::errc::value_too_large);
        return lua_error(L);
    }

    try {
        *tp -= std::chrono::round<std::chrono::system_clock::duration>(dur);
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }
    return 0;
}

inline int system_clock_time_point_seconds_since_epoch(lua_State* L)
{
    auto tp = static_cast<std::chrono::system_clock::time_point*>(
        lua_touserdata(L, 1));
    lua_pushnumber(L, lua_Seconds{tp->time_since_epoch()}.count());
    return 1;
}

static int system_clock_time_point_mt_index(lua_State* L)
{
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("add"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, system_clock_time_point_add);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("sub"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, system_clock_time_point_sub);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("seconds_since_epoch"),
                system_clock_time_point_seconds_since_epoch
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

static int system_clock_time_point_mt_eq(lua_State* L)
{
    auto tp1 = static_cast<std::chrono::system_clock::time_point*>(
        lua_touserdata(L, 1));
    auto tp2 = static_cast<std::chrono::system_clock::time_point*>(
        lua_touserdata(L, 2));
    lua_pushboolean(L, *tp1 == *tp2);
    return 1;
}

static int system_clock_time_point_mt_lt(lua_State* L)
{
    auto tp1 = static_cast<std::chrono::system_clock::time_point*>(
        lua_touserdata(L, 1));
    if (!tp1 || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &system_clock_time_point_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto tp2 = static_cast<std::chrono::system_clock::time_point*>(
        lua_touserdata(L, 2));
    if (!tp2 || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &system_clock_time_point_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    lua_pushboolean(L, *tp1 < *tp2);
    return 1;
}

static int system_clock_time_point_mt_le(lua_State* L)
{
    auto tp1 = static_cast<std::chrono::system_clock::time_point*>(
        lua_touserdata(L, 1));
    if (!tp1 || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &system_clock_time_point_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto tp2 = static_cast<std::chrono::system_clock::time_point*>(
        lua_touserdata(L, 2));
    if (!tp2 || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &system_clock_time_point_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    lua_pushboolean(L, *tp1 <= *tp2);
    return 1;
}

static int system_clock_time_point_mt_add(lua_State* L)
{
    auto tp = static_cast<std::chrono::system_clock::time_point*>(
        lua_touserdata(L, 1));
    if (!tp || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &system_clock_time_point_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    lua_Number secs = luaL_checknumber(L, 2);
    if (std::isnan(secs) || std::isinf(secs)) {
        push(L, std::errc::argument_out_of_domain, "arg", 2);
        return lua_error(L);
    }

    lua_Seconds dur{secs};
    if (
        dur > std::chrono::system_clock::duration::max() ||
        dur < std::chrono::system_clock::duration::min()
    ) {
        push(L, std::errc::value_too_large);
        return lua_error(L);
    }

    auto ret = static_cast<std::chrono::system_clock::time_point*>(
        lua_newuserdata(L, sizeof(std::chrono::system_clock::time_point))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &system_clock_time_point_mt_key);
    setmetatable(L, -2);
    new (ret) std::chrono::system_clock::time_point{};

    try {
        *ret = *tp +
            std::chrono::round<std::chrono::system_clock::duration>(dur);
        return 1;
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }
}

static int system_clock_time_point_mt_sub(lua_State* L)
{
    auto tp = static_cast<std::chrono::system_clock::time_point*>(
        lua_touserdata(L, 1));
    if (!tp || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &system_clock_time_point_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    switch (lua_type(L, 2)) {
    case LUA_TNUMBER: {
        lua_Number secs = lua_tonumber(L, 2);
        if (std::isnan(secs) || std::isinf(secs)) {
            push(L, std::errc::argument_out_of_domain, "arg", 2);
            return lua_error(L);
        }

        lua_Seconds dur{secs};
        if (
            dur > std::chrono::system_clock::duration::max() ||
            dur < std::chrono::system_clock::duration::min()
        ) {
            push(L, std::errc::value_too_large);
            return lua_error(L);
        }

        auto ret = static_cast<std::chrono::system_clock::time_point*>(
            lua_newuserdata(L, sizeof(std::chrono::system_clock::time_point))
        );
        rawgetp(L, LUA_REGISTRYINDEX, &system_clock_time_point_mt_key);
        setmetatable(L, -2);
        new (ret) std::chrono::system_clock::time_point{};

        try {
            *ret = *tp -
                std::chrono::round<std::chrono::system_clock::duration>(dur);
            return 1;
        } catch (const std::system_error& e) {
            push(L, e.code());
            return lua_error(L);
        } catch (const std::exception& e) {
            lua_pushstring(L, e.what());
            return lua_error(L);
        }
    }
    case LUA_TUSERDATA: {
        auto tp2 = static_cast<std::chrono::system_clock::time_point*>(
            lua_touserdata(L, 2));
        if (!tp2 || !lua_getmetatable(L, 2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &system_clock_time_point_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        try {
            lua_pushnumber(L, lua_Seconds{*tp - *tp2}.count());
            return 1;
        } catch (const std::system_error& e) {
            push(L, e.code());
            return lua_error(L);
        } catch (const std::exception& e) {
            lua_pushstring(L, e.what());
            return lua_error(L);
        }
    }
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
}

static int system_clock_epoch(lua_State* L)
{
    auto tp = static_cast<std::chrono::system_clock::time_point*>(
        lua_newuserdata(L, sizeof(std::chrono::system_clock::time_point))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &system_clock_time_point_mt_key);
    setmetatable(L, -2);
    new (tp) std::chrono::system_clock::time_point{};
    return 1;
}

static int system_clock_now(lua_State* L)
{
    auto tp = static_cast<std::chrono::system_clock::time_point*>(
        lua_newuserdata(L, sizeof(std::chrono::system_clock::time_point))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &system_clock_time_point_mt_key);
    setmetatable(L, -2);
    new (tp) std::chrono::system_clock::time_point{};
    *tp = std::chrono::system_clock::now();
    return 1;
}

static int system_timer_wait(lua_State* L)
{
    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto handle = static_cast<handle_type<std::chrono::system_clock>*>(
        lua_touserdata(L, 1));
    if (!handle || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &system_timer_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto cancel_slot = set_default_interrupter(L, *vm_ctx);

    handle->timer.async_wait(
        asio::bind_cancellation_slot(cancel_slot, asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber](const boost::system::error_code& ec) {
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

static int system_timer_expires_at(lua_State* L)
{
    auto handle = static_cast<handle_type<std::chrono::system_clock>*>(
        lua_touserdata(L, 1));
    if (!handle || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &system_timer_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto tp = static_cast<std::chrono::system_clock::time_point*>(
        lua_touserdata(L, 2));
    if (!tp || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &system_clock_time_point_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    try {
        auto n = handle->timer.expires_at(*tp);
        lua_pushinteger(L, n);
        return 1;
    } catch (const boost::system::system_error& e) {
        push(L, static_cast<std::error_code>(e.code()));
        return lua_error(L);
    }
}

static int system_timer_cancel(lua_State* L)
{
    auto handle = static_cast<handle_type<std::chrono::system_clock>*>(
        lua_touserdata(L, 1));
    if (!handle || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &system_timer_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    try {
        auto n = handle->timer.cancel();
        lua_pushinteger(L, n);
        return 1;
    } catch (const boost::system::system_error& e) {
        push(L, static_cast<std::error_code>(e.code()));
        return lua_error(L);
    }
}

inline int system_timer_expiry(lua_State* L)
{
    auto handle = static_cast<handle_type<std::chrono::system_clock>*>(
        lua_touserdata(L, 1));

    auto tp = static_cast<std::chrono::system_clock::time_point*>(
        lua_newuserdata(L, sizeof(std::chrono::system_clock::time_point))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &system_clock_time_point_mt_key);
    setmetatable(L, -2);
    new (tp) std::chrono::system_clock::time_point{};
    *tp = handle->timer.expiry();
    return 1;
}

static int system_timer_mt_index(lua_State* L)
{
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("wait"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX, &system_timer_wait_key);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("expires_at"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, system_timer_expires_at);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("cancel"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, system_timer_cancel);
                    return 1;
                }
            ),
            hana::make_pair(BOOST_HANA_STRING("expiry"), system_timer_expiry)
        ),
        [](std::string_view /*key*/, lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        },
        tostringview(L, 2),
        L
    );
}

static int system_timer_new(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);
    auto buf = static_cast<handle_type<std::chrono::system_clock>*>(
        lua_newuserdata(L, sizeof(handle_type<std::chrono::system_clock>))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &system_timer_mt_key);
    setmetatable(L, -2);
    new (buf) handle_type<std::chrono::system_clock>{vm_ctx.strand().context()};
    return 1;
}

void init_time(lua_State* L)
{
    lua_pushlightuserdata(L, &steady_clock_time_point_mt_key);
    {
        static_assert(std::is_trivially_destructible_v<
            std::chrono::steady_clock::time_point>);

        lua_createtable(L, /*narr=*/0, /*nrec=*/7);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "steady_clock.time_point");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, steady_clock_time_point_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__eq");
        lua_pushcfunction(L, steady_clock_time_point_mt_eq);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__lt");
        lua_pushcfunction(L, steady_clock_time_point_mt_lt);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__le");
        lua_pushcfunction(L, steady_clock_time_point_mt_le);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__add");
        lua_pushcfunction(L, steady_clock_time_point_mt_add);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__sub");
        lua_pushcfunction(L, steady_clock_time_point_mt_sub);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &high_resolution_clock_time_point_mt_key);
    {
        static_assert(std::is_trivially_destructible_v<
            std::chrono::high_resolution_clock::time_point>);

        lua_createtable(L, /*narr=*/0, /*nrec=*/6);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "high_resolution_clock.time_point");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, high_resolution_clock_time_point_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__eq");
        lua_pushcfunction(L, high_resolution_clock_time_point_mt_eq);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__lt");
        lua_pushcfunction(L, high_resolution_clock_time_point_mt_lt);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__le");
        lua_pushcfunction(L, high_resolution_clock_time_point_mt_le);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__sub");
        lua_pushcfunction(L, high_resolution_clock_time_point_mt_sub);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &steady_timer_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "steady_timer");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, steady_timer_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<handle_type<std::chrono::steady_clock>>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &system_clock_time_point_mt_key);
    {
        static_assert(std::is_trivially_destructible_v<
            std::chrono::system_clock::time_point>);

        lua_createtable(L, /*narr=*/0, /*nrec=*/7);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "system_clock.time_point");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, system_clock_time_point_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__eq");
        lua_pushcfunction(L, system_clock_time_point_mt_eq);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__lt");
        lua_pushcfunction(L, system_clock_time_point_mt_lt);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__le");
        lua_pushcfunction(L, system_clock_time_point_mt_le);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__add");
        lua_pushcfunction(L, system_clock_time_point_mt_add);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__sub");
        lua_pushcfunction(L, system_clock_time_point_mt_sub);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &system_timer_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "system_timer");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, system_timer_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<handle_type<std::chrono::system_clock>>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &time_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/6);

        lua_pushliteral(L, "steady_clock");
        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/2);

            lua_pushliteral(L, "epoch");
            lua_pushcfunction(L, steady_clock_epoch);
            lua_rawset(L, -3);

            lua_pushliteral(L, "now");
            lua_pushcfunction(L, steady_clock_now);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);

        lua_pushliteral(L, "steady_timer");
        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/1);

            lua_pushliteral(L, "new");
            lua_pushcfunction(L, steady_timer_new);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);

        lua_pushliteral(L, "system_clock");
        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/2);

            lua_pushliteral(L, "epoch");
            lua_pushcfunction(L, system_clock_epoch);
            lua_rawset(L, -3);

            lua_pushliteral(L, "now");
            lua_pushcfunction(L, system_clock_now);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);

        lua_pushliteral(L, "system_timer");
        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/1);

            lua_pushliteral(L, "new");
            lua_pushcfunction(L, system_timer_new);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);

        lua_pushliteral(L, "high_resolution_clock");
        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/3);

            lua_pushliteral(L, "epoch");
            lua_pushcfunction(L, high_resolution_clock_epoch);
            lua_rawset(L, -3);

            lua_pushliteral(L, "now");
            lua_pushcfunction(L, high_resolution_clock_now);
            lua_rawset(L, -3);

            lua_pushliteral(L, "is_steady");
            lua_pushboolean(L, std::chrono::high_resolution_clock::is_steady);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);

        {
            lua_pushlightuserdata(L, &steady_timer_wait_key);
            rawgetp(L, LUA_REGISTRYINDEX,
                    &var_args__retval1_to_error__fwd_retval2__key);
            lua_pushvalue(L, -1);
            lua_insert(L, -3);
            rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
            lua_pushcfunction(L, steady_timer_wait);
            lua_call(L, 2, 1);
            lua_rawset(L, LUA_REGISTRYINDEX);

            lua_pushlightuserdata(L, &system_timer_wait_key);
            lua_pushvalue(L, -2);
            rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
            lua_pushcfunction(L, system_timer_wait);
            lua_call(L, 2, 1);
            lua_rawset(L, LUA_REGISTRYINDEX);

            lua_pushliteral(L, "sleep");
            lua_insert(L, -2);
            rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
            lua_pushcfunction(L, sleep_for);
            lua_call(L, 2, 1);
            lua_rawset(L, -3);
        }
    }
    lua_rawset(L, LUA_REGISTRYINDEX);
}

} // namespace emilua
