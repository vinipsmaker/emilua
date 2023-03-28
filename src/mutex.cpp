/* Copyright (c) 2020 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

EMILUA_GPERF_DECLS_BEGIN(includes)
#include <fmt/format.h>

#include <emilua/mutex.hpp>
EMILUA_GPERF_DECLS_END(includes)

namespace emilua {

char mutex_key;
char mutex_mt_key;

inline mutex_handle::mutex_handle(vm_context& vm_ctx)
    : vm_ctx{vm_ctx}
{}

inline mutex_handle::~mutex_handle()
{
    constexpr auto spec{FMT_STRING(
        "No scheduled fibers remaining to unlock mutex {}"
    )};
    if (pending.size() != 0)
        vm_ctx.notify_deadlock(fmt::format(spec, static_cast<void*>(this)));
}

EMILUA_GPERF_DECLS_BEGIN(mutex)
EMILUA_GPERF_NAMESPACE(emilua)
static int mutex_lock(lua_State* L)
{
    auto handle = static_cast<mutex_handle*>(lua_touserdata(L, 1));
    if (!handle || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &mutex_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto& vm_ctx = handle->vm_ctx;
    EMILUA_CHECK_SUSPEND_ALLOWED_ASSUMING_INTERRUPTION_DISABLED(vm_ctx, L);

    if (handle->locked) {
        handle->pending.emplace_back(vm_ctx.current_fiber());
        return lua_yield(L, 0);
    }

    handle->locked = true;
    return 0;
}

static int mutex_unlock(lua_State* L)
{
    auto handle = static_cast<mutex_handle*>(lua_touserdata(L, 1));
    if (!handle || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &mutex_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    if (!handle->locked) {
        push(L, std::errc::operation_not_permitted);
        return lua_error(L);
    }

    if (handle->pending.size() == 0) {
        handle->locked = false;
        return 0;
    }

    auto vm_ctx = handle->vm_ctx.shared_from_this();
    auto next = handle->pending.front();
    handle->pending.pop_front();
    vm_ctx->strand().post([vm_ctx,next]() {
        vm_ctx->fiber_resume(
            next, hana::make_set(vm_context::options::skip_clear_interrupter));
    }, std::allocator<void>{});
    return 0;
}
EMILUA_GPERF_DECLS_END(mutex)

static int mutex_mt_index(lua_State* L)
{
    auto key = tostringview(L, 2);
    return EMILUA_GPERF_BEGIN(key)
        EMILUA_GPERF_PARAM(int (*action)(lua_State*))
        EMILUA_GPERF_DEFAULT_VALUE([](lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        })
        EMILUA_GPERF_PAIR(
            "unlock",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, mutex_unlock);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "lock",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, mutex_lock);
                return 1;
            })
    EMILUA_GPERF_END(key)(L);
}

static int mutex_new(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);
    auto buf = static_cast<mutex_handle*>(
        lua_newuserdata(L, sizeof(mutex_handle))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &mutex_mt_key);
    setmetatable(L, -2);
    new (buf) mutex_handle{vm_ctx};
    return 1;
}

void init_mutex_module(lua_State* L)
{
    lua_pushlightuserdata(L, &mutex_key);
    lua_newtable(L);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "mutex");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(
            L,
            [](lua_State* L) -> int {
                auto key = tostringview(L, 2);
                if (key == "new") {
                    lua_pushcfunction(L, mutex_new);
                    return 1;
                } else {
                    push(L, errc::bad_index, "index", 2);
                    return lua_error(L);
                }
            });
        lua_rawset(L, -3);

        lua_pushliteral(L, "__newindex");
        lua_pushcfunction(
            L,
            [](lua_State* L) -> int {
                push(L, std::errc::operation_not_permitted);
                return lua_error(L);
            });
        lua_rawset(L, -3);
    }
    setmetatable(L, -2);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &mutex_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "mutex");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, mutex_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<mutex_handle>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);
}

} // namespace emilua
