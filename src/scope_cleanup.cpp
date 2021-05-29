/* Copyright (c) 2020 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#include <emilua/scope_cleanup.hpp>
#include <emilua/fiber.hpp>

namespace emilua {

extern unsigned char scope_bytecode[];
extern std::size_t scope_bytecode_size;
extern unsigned char scope_cleanup_pop_bytecode[];
extern std::size_t scope_cleanup_pop_bytecode_size;

char scope_cleanup_handlers_key;

static void disable_interruption(lua_State* L)
{
    auto current_fiber = get_vm_context(L).current_fiber();

    rawgetp(L, LUA_REGISTRYINDEX, &fiber_list_key);
    lua_pushthread(current_fiber);
    lua_xmove(current_fiber, L, 1);
    lua_rawget(L, -2);
    lua_rawgeti(L, -1, FiberDataIndex::INTERRUPTION_DISABLED);
    auto count = lua_tointeger(L, -1);
    ++count;
    assert(count >= 0); //< TODO: better overflow detection and VM shutdown
    lua_pushinteger(L, count);
    lua_rawseti(L, -3, FiberDataIndex::INTERRUPTION_DISABLED);
    lua_pop(L, 3);
}

static int restore_interruption(lua_State* L)
{
    auto current_fiber = get_vm_context(L).current_fiber();

    rawgetp(L, LUA_REGISTRYINDEX, &fiber_list_key);
    lua_pushthread(current_fiber);
    lua_xmove(current_fiber, L, 1);
    lua_rawget(L, -2);
    lua_rawgeti(L, -1, FiberDataIndex::INTERRUPTION_DISABLED);
    auto count = lua_tointeger(L, -1);
    if (!(count > 0)) {
        push(L, errc::interruption_already_allowed);
        return lua_error(L);
    }
    lua_pushinteger(L, --count);
    lua_rawseti(L, -3, FiberDataIndex::INTERRUPTION_DISABLED);
    return 0;
}

static int scope_cleanup_push(lua_State* L)
{
    if (lua_type(L, 1) != LUA_TFUNCTION) {
        push(L, std::errc::invalid_argument);
        return lua_error(L);
    }

    rawgetp(L, LUA_REGISTRYINDEX, &scope_cleanup_handlers_key);
    lua_pushthread(L);
    lua_rawget(L, -2);
    lua_rawgeti(L, -1, (int)lua_objlen(L, -1));
    auto len = (int)lua_objlen(L, -1);
    lua_pushvalue(L, 1);
    lua_rawseti(L, -2, len + 1);
    return 0;
}

static int scope_cleanup_pop(lua_State* L)
{
    rawgetp(L, LUA_REGISTRYINDEX, &scope_cleanup_handlers_key);
    lua_pushthread(L);
    lua_rawget(L, -2);
    lua_rawgeti(L, -1, (int)lua_objlen(L, -1));
    auto len = (int)lua_objlen(L, -1);
    lua_rawgeti(L, -1, len);
    lua_pushnil(L);
    lua_rawseti(L, -3, len);
    if (lua_type(L, -1) == LUA_TNIL) {
        push(L, errc::unmatched_scope_cleanup);
        return lua_error(L);
    }
    disable_interruption(L);
    assert(lua_type(L, -1) == LUA_TFUNCTION);
    return 1;
}

void init_scope_cleanup_module(lua_State* L)
{
    lua_pushlightuserdata(L, &scope_cleanup_handlers_key);
    lua_newtable(L);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/1);

        lua_pushliteral(L, "__mode");
        lua_pushliteral(L, "k");
        lua_rawset(L, -3);

        setmetatable(L, -2);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    init_new_coro_or_fiber_scope(L, L);

    lua_pushliteral(L, "scope");
    int res = luaL_loadbuffer(L, reinterpret_cast<char*>(scope_bytecode),
                              scope_bytecode_size, nullptr);
    assert(res == 0); boost::ignore_unused(res);
    lua_pushcfunction(L, unsafe_scope_push);
    lua_pushcfunction(L, unsafe_scope_pop);
    lua_pushcfunction(L, terminate_vm_with_cleanup_error);
    lua_pushcfunction(L, restore_interruption);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_pcall_key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_call(L, 6, 1);
    lua_rawset(L, LUA_GLOBALSINDEX);
    lua_pushliteral(L, "scope_cleanup_push");
    lua_pushcfunction(L, scope_cleanup_push);
    lua_rawset(L, LUA_GLOBALSINDEX);
    lua_pushliteral(L, "scope_cleanup_pop");
    res = luaL_loadbuffer(
        L, reinterpret_cast<char*>(scope_cleanup_pop_bytecode),
        scope_cleanup_pop_bytecode_size, nullptr);
    assert(res == 0); boost::ignore_unused(res);
    lua_pushcfunction(L, scope_cleanup_pop);
    lua_pushcfunction(L, restore_interruption);
    lua_pushcfunction(L, terminate_vm_with_cleanup_error);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_pcall_key);
    lua_call(L, 4, 1);
    lua_rawset(L, LUA_GLOBALSINDEX);
}

void init_new_coro_or_fiber_scope(lua_State* L, lua_State* from)
{
    rawgetp(from, LUA_REGISTRYINDEX, &scope_cleanup_handlers_key);
    lua_pushthread(L);
    lua_xmove(L, from, 1);
    lua_newtable(from);
    {
        lua_newtable(from);
        lua_rawseti(from, -2, 1);
    }
    lua_rawset(from, -3);
    lua_pop(from, 1);
}

int unsafe_scope_push(lua_State* L)
{
    rawgetp(L, LUA_REGISTRYINDEX, &scope_cleanup_handlers_key);
    lua_pushthread(L);
    lua_rawget(L, -2);
    auto len = (int)lua_objlen(L, -1);
    lua_newtable(L);
    lua_rawseti(L, -2, len + 1);
    return 0;
}

int unsafe_scope_pop(lua_State* L)
{
    disable_interruption(L);

    rawgetp(L, LUA_REGISTRYINDEX, &scope_cleanup_handlers_key);
    lua_pushthread(L);
    lua_rawget(L, -2);
    auto len = (int)lua_objlen(L, -1);
    lua_rawgeti(L, -1, len);
    lua_pushnil(L);
    lua_rawseti(L, -3, len);
    return 1;
}

int terminate_vm_with_cleanup_error(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);
    vm_ctx.notify_cleanup_error(L);
    return lua_yield(L, 0);
}

int root_scope(lua_State* L)
{
    disable_interruption(L);

    rawgetp(L, LUA_REGISTRYINDEX, &scope_cleanup_handlers_key);
    lua_pushthread(L);
    lua_rawget(L, -2);
    lua_rawgeti(L, -1, 1);
    return 1;
}

} // namespace emilua
