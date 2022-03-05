/* Copyright (c) 2022 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#include <emilua/async_base.hpp>

namespace emilua {

extern unsigned char var_args__retval1_to_error__fwd_retval2__bytecode[];
extern std::size_t var_args__retval1_to_error__fwd_retval2__bytecode_size;
extern unsigned char var_args__retval1_to_error__bytecode[];
extern std::size_t var_args__retval1_to_error__bytecode_size;
extern unsigned char var_args__retval1_to_error__fwd_retval234__bytecode[];
extern std::size_t var_args__retval1_to_error__fwd_retval234__bytecode_size;
extern unsigned char var_args__retval1_to_error__fwd_retval23__bytecode[];
extern std::size_t var_args__retval1_to_error__fwd_retval23__bytecode_size;

char var_args__retval1_to_error__fwd_retval2__key;
char var_args__retval1_to_error__key;
char var_args__retval1_to_error__fwd_retval234__key;
char var_args__retval1_to_error__fwd_retval23__key;

void init_async_base(lua_State* L)
{
    lua_pushlightuserdata(L, &var_args__retval1_to_error__fwd_retval2__key);
    int res = luaL_loadbuffer(
        L,
        reinterpret_cast<char*>(
            var_args__retval1_to_error__fwd_retval2__bytecode),
        var_args__retval1_to_error__fwd_retval2__bytecode_size, nullptr);
    assert(res == 0); boost::ignore_unused(res);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &var_args__retval1_to_error__key);
    res = luaL_loadbuffer(
        L,
        reinterpret_cast<char*>(var_args__retval1_to_error__bytecode),
        var_args__retval1_to_error__bytecode_size, nullptr);
    assert(res == 0); boost::ignore_unused(res);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &var_args__retval1_to_error__fwd_retval234__key);
    res = luaL_loadbuffer(
        L,
        reinterpret_cast<char*>(
            var_args__retval1_to_error__fwd_retval234__bytecode),
        var_args__retval1_to_error__fwd_retval234__bytecode_size, nullptr);
    assert(res == 0); boost::ignore_unused(res);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &var_args__retval1_to_error__fwd_retval23__key);
    res = luaL_loadbuffer(
        L,
        reinterpret_cast<char*>(
            var_args__retval1_to_error__fwd_retval23__bytecode),
        var_args__retval1_to_error__fwd_retval23__bytecode_size, nullptr);
    assert(res == 0); boost::ignore_unused(res);
    lua_rawset(L, LUA_REGISTRYINDEX);
}

} // namespace emilua
