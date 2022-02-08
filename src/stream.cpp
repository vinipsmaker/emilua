/* Copyright (c) 2022 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#include <emilua/stream.hpp>

namespace emilua {

extern unsigned char write_bytecode[];
extern std::size_t write_bytecode_size;
extern unsigned char read_bytecode[];
extern std::size_t read_bytecode_size;

char stream_key;

void init_stream(lua_State* L)
{
    int res;

    lua_pushlightuserdata(L, &stream_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/2);

        lua_pushliteral(L, "write");
        res = luaL_loadbuffer(L, reinterpret_cast<char*>(write_bytecode),
                              write_bytecode_size, nullptr);
        assert(res == 0); boost::ignore_unused(res);
        lua_rawset(L, -3);

        lua_pushliteral(L, "read");
        res = luaL_loadbuffer(L, reinterpret_cast<char*>(read_bytecode),
                              read_bytecode_size, nullptr);
        assert(res == 0); boost::ignore_unused(res);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);
}

} // namespace emilua
