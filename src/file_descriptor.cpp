/* Copyright (c) 2022 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#include <emilua/file_descriptor.hpp>

namespace emilua {

char file_descriptor_mt_key;

static int file_descriptor_close(lua_State* L)
{
    auto handle = reinterpret_cast<file_descriptor_handle*>(
        lua_touserdata(L, 1));
    if (!handle || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    if (*handle == INVALID_FILE_DESCRIPTOR) {
        push(L, std::errc::device_or_resource_busy);
        return lua_error(L);
    }

    lua_pushnil(L);
    setmetatable(L, 1);

    int res = close(*handle);
    boost::ignore_unused(res);

    return 0;
}

static int file_descriptor_mt_index(lua_State* L)
{
    if (tostringview(L, 2) != "close") {
        push(L, errc::bad_index, "index", 2);
        return lua_error(L);
    }

    lua_pushcfunction(L, file_descriptor_close);
    return 1;
}

static int file_descriptor_mt_gc(lua_State* L)
{
    auto& handle = *reinterpret_cast<file_descriptor_handle*>(
        lua_touserdata(L, 1));
    if (handle == INVALID_FILE_DESCRIPTOR)
        return 0;

    int res = close(handle);
    boost::ignore_unused(res);
    return 0;
}

void init_file_descriptor(lua_State* L)
{
    lua_pushlightuserdata(L, &file_descriptor_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "file_descriptor");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, file_descriptor_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, file_descriptor_mt_gc);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);
}

} // namespace emilua
