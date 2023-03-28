/* Copyright (c) 2020 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

EMILUA_GPERF_DECLS_BEGIN(includes)
#include <emilua/condition_variable.hpp>
#include <emilua/mutex.hpp>
EMILUA_GPERF_DECLS_END(includes)

namespace emilua {

extern unsigned char cond_wait_bytecode[];
extern std::size_t cond_wait_bytecode_size;

char condition_variable_key;

EMILUA_GPERF_DECLS_BEGIN(condition_variable)
EMILUA_GPERF_NAMESPACE(emilua)
static char cond_mt_key;
static char cond_wait_key;

struct cond_handle
{
    std::deque<lua_State*> pending;
};
EMILUA_GPERF_DECLS_END(condition_variable)

static int cond_wait(lua_State* L)
{
    auto cond_handle = static_cast<struct cond_handle*>(lua_touserdata(L, 1));
    if (!cond_handle || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &cond_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto mutex_handle = static_cast<struct mutex_handle*>(lua_touserdata(L, 2));
    if (!mutex_handle || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &mutex_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    if (!mutex_handle->locked) {
        push(L, std::errc::operation_not_permitted);
        return lua_error(L);
    }

    EMILUA_CHECK_SUSPEND_ALLOWED(mutex_handle->vm_ctx, L);

    lua_pushvalue(L, 1);
    lua_pushlightuserdata(L, mutex_handle->vm_ctx.current_fiber());
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto vm_ctx = get_vm_context(L).shared_from_this();
            auto handle = static_cast<struct cond_handle*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            auto current_fiber = static_cast<lua_State*>(
                lua_touserdata(L, lua_upvalueindex(2)));

            auto it = std::find(handle->pending.begin(), handle->pending.end(),
                                current_fiber);
            if (it == handle->pending.end()) {
                // notify() was called before interrupt()
                //
                // an alternative would be to clear the interrupter on notify(),
                // but this approach works well enough
                return 0;
            }

            handle->pending.erase(it);
            vm_ctx->strand().post([vm_ctx,current_fiber]() {
                auto opt_args = vm_context::options::arguments;
                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        hana::make_pair(
                            opt_args, hana::make_tuple(errc::interrupted))));
            }, std::allocator<void>{});
            return 0;
        },
        2);
    set_interrupter(L, mutex_handle->vm_ctx);

    cond_handle->pending.emplace_back(mutex_handle->vm_ctx.current_fiber());

    // mutex:unlock() {{{
    if (mutex_handle->pending.size() == 0) {
        mutex_handle->locked = false;
    } else {
        auto vm_ctx = mutex_handle->vm_ctx.shared_from_this();
        auto next = mutex_handle->pending.front();
        mutex_handle->pending.pop_front();
        vm_ctx->strand().post([vm_ctx,next]() {
            vm_ctx->fiber_resume(
                next,
                hana::make_set(vm_context::options::skip_clear_interrupter));
        }, std::allocator<void>{});
    }
    // }}}

    return lua_yield(L, 0);
}

EMILUA_GPERF_DECLS_BEGIN(condition_variable)
EMILUA_GPERF_NAMESPACE(emilua)
static int cond_notify_one(lua_State* L)
{
    auto handle = static_cast<cond_handle*>(lua_touserdata(L, 1));
    if (!handle || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &cond_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    if (handle->pending.size() == 0)
        return 0;

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto next = handle->pending.front();
    handle->pending.pop_front();
    vm_ctx->strand().post([vm_ctx,next]() {
        vm_ctx->fiber_resume(next);
    }, std::allocator<void>{});
    return 0;
}

static int cond_notify_all(lua_State* L)
{
    auto handle = static_cast<cond_handle*>(lua_touserdata(L, 1));
    if (!handle || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &cond_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto vm_ctx = get_vm_context(L).shared_from_this();
    for (auto& p: handle->pending) {
        vm_ctx->strand().post([vm_ctx,p]() {
            vm_ctx->fiber_resume(p);
        }, std::allocator<void>{});
    }
    handle->pending.clear();
    return 0;
}
EMILUA_GPERF_DECLS_END(condition_variable)

static int cond_mt_index(lua_State* L)
{
    auto key = tostringview(L, 2);
    return EMILUA_GPERF_BEGIN(key)
        EMILUA_GPERF_PARAM(int (*action)(lua_State*))
        EMILUA_GPERF_DEFAULT_VALUE([](lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        })
        EMILUA_GPERF_PAIR(
            "notify_all",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, cond_notify_all);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "wait",
            [](lua_State* L) -> int {
                rawgetp(L, LUA_REGISTRYINDEX, &cond_wait_key);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "notify_one",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, cond_notify_one);
                return 1;
            })
    EMILUA_GPERF_END(key)(L);
}

static int cond_new(lua_State* L)
{
    auto buf = static_cast<cond_handle*>(
        lua_newuserdata(L, sizeof(cond_handle))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &cond_mt_key);
    setmetatable(L, -2);
    new (buf) cond_handle{};
    return 1;
}

void init_condition_variable_module(lua_State* L)
{
    lua_pushlightuserdata(L, &condition_variable_key);
    lua_newtable(L);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "condition_variable");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(
            L,
            [](lua_State* L) -> int {
                auto key = tostringview(L, 2);
                if (key == "new") {
                    lua_pushcfunction(L, cond_new);
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

    lua_pushlightuserdata(L, &cond_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "condition_variable");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, cond_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<cond_handle>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    {
        lua_pushlightuserdata(L, &cond_wait_key);
        int res = luaL_loadbuffer(
            L, reinterpret_cast<char*>(cond_wait_bytecode),
            cond_wait_bytecode_size, nullptr);
        assert(res == 0); boost::ignore_unused(res);
        rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
        lua_pushcfunction(L, cond_wait);
        lua_call(L, 2, 1);
        lua_rawset(L, LUA_REGISTRYINDEX);
    }
}

} // namespace emilua
