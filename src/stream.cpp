/* Copyright (c) 2022 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#include <emilua/stream.hpp>

#include <regex>

namespace emilua {

extern unsigned char stream_connect_bytecode[];
extern std::size_t stream_connect_bytecode_size;
extern unsigned char write_all_bytecode[];
extern std::size_t write_all_bytecode_size;
extern unsigned char read_all_bytecode[];
extern std::size_t read_all_bytecode_size;
extern unsigned char get_line_bytecode[];
extern std::size_t get_line_bytecode_size;
extern unsigned char scanner_buffer_bytecode[];
extern std::size_t scanner_buffer_bytecode_size;
extern unsigned char scanner_set_buffer_bytecode[];
extern std::size_t scanner_set_buffer_bytecode_size;
extern unsigned char scanner_remove_line_bytecode[];
extern std::size_t scanner_remove_line_bytecode_size;
extern unsigned char scanner_buffered_line_bytecode[];
extern std::size_t scanner_buffered_line_bytecode_size;
extern unsigned char scanner_new_bytecode[];
extern std::size_t scanner_new_bytecode_size;
extern unsigned char scanner_with_awk_defaults_bytecode[];
extern std::size_t scanner_with_awk_defaults_bytecode_size;

char stream_key;

int byte_span_new(lua_State* L);
int regex_new(lua_State* L);
int regex_search(lua_State* L);
int regex_split(lua_State* L);
int regex_patsplit(lua_State* L);

static constexpr auto re_search_flags =
    // Emilua behaves different than GAWK:
    //
    // > https://www.gnu.org/software/gawk/manual/html_node/gawk-split-records.html
    // > Remember that in awk, the ‘^’ and ‘$’ anchor metacharacters match the
    // > beginning and end of a string, and not the beginning and end of a
    // > line. As a result, something like ‘RS = "^[[:upper:]]"’ can only match
    // > at the beginning of a file.
    //
    // The rationale for a different behavior is simple: It's never reliable to
    // match the end-of-line as EOF could've been signalled just after the call
    // to regex_search()/read_some(). Therefore we forbid the feature
    // altogether.
    std::regex_constants::match_not_bol |
    std::regex_constants::match_not_eol |
    std::regex_constants::match_not_bow |
    std::regex_constants::match_not_eow |
    // Emilua behaves as in GAWK:
    //
    // > https://www.gnu.org/software/gawk/manual/html_node/gawk-split-records.html
    // > record splitting does not [allow a regexp to match the empty
    // > string]. Thus, for example ‘RS = "()"’ does not split records between
    // > characters.
    //
    // The rationale is the same one found for FS.
    std::regex_constants::match_not_null;

void init_stream(lua_State* L)
{
    int res;

    lua_pushlightuserdata(L, &stream_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/4);

        lua_pushliteral(L, "connect");
        res = luaL_loadbuffer(
            L, reinterpret_cast<char*>(stream_connect_bytecode),
            stream_connect_bytecode_size, nullptr);
        assert(res == 0); boost::ignore_unused(res);
        rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
        rawgetp(L, LUA_REGISTRYINDEX, &raw_type_key);
        rawgetp(L, LUA_REGISTRYINDEX, &raw_next_key);
        rawgetp(L, LUA_REGISTRYINDEX, &raw_pcall_key);
        push(L, make_error_code(asio::error::not_found));
        push(L, std::errc::invalid_argument);
        lua_call(L, 6, 1);
        lua_rawset(L, -3);

        lua_pushliteral(L, "write_all");
        res = luaL_loadbuffer(L, reinterpret_cast<char*>(write_all_bytecode),
                              write_all_bytecode_size, nullptr);
        assert(res == 0); boost::ignore_unused(res);
        lua_rawset(L, -3);

        lua_pushliteral(L, "read_all");
        res = luaL_loadbuffer(L, reinterpret_cast<char*>(read_all_bytecode),
                              read_all_bytecode_size, nullptr);
        assert(res == 0); boost::ignore_unused(res);
        lua_rawset(L, -3);

        lua_pushliteral(L, "scanner");
        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/3);

            {
                lua_createtable(L, /*narr=*/0, /*nrec=*/1);

                lua_pushliteral(L, "__index");
                {
                    lua_createtable(L, /*narr=*/0, /*nrec=*/5);

                    lua_pushliteral(L, "get_line");
                    {
                        res = luaL_loadbuffer(
                            L, reinterpret_cast<char*>(get_line_bytecode),
                            get_line_bytecode_size, nullptr);
                        assert(res == 0); boost::ignore_unused(res);
                        rawgetp(L, LUA_REGISTRYINDEX, &raw_type_key);
                        rawgetp(L, LUA_REGISTRYINDEX, &raw_getmetatable_key);
                        rawgetp(L, LUA_REGISTRYINDEX, &raw_pcall_key);
                        rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
                        lua_pushcfunction(L, byte_span_new);
                        lua_pushcfunction(L, regex_search);
                        lua_pushinteger(L, re_search_flags);
                        lua_pushcfunction(L, regex_split);
                        lua_pushcfunction(L, regex_patsplit);
                        push(L, make_error_code(asio::error::eof));
                        push(L, std::errc::message_size);
                        lua_call(L, 11, 1);
                    }
                    lua_rawset(L, -3);

                    lua_pushliteral(L, "buffer");
                    res = luaL_loadbuffer(
                        L, reinterpret_cast<char*>(scanner_buffer_bytecode),
                        scanner_buffer_bytecode_size, nullptr);
                    assert(res == 0); boost::ignore_unused(res);
                    lua_rawset(L, -3);

                    lua_pushliteral(L, "set_buffer");
                    res = luaL_loadbuffer(
                        L, reinterpret_cast<char*>(scanner_set_buffer_bytecode),
                        scanner_set_buffer_bytecode_size, nullptr);
                    assert(res == 0); boost::ignore_unused(res);
                    lua_rawset(L, -3);

                    lua_pushliteral(L, "remove_line");
                    res = luaL_loadbuffer(
                        L,
                        reinterpret_cast<char*>(scanner_remove_line_bytecode),
                        scanner_remove_line_bytecode_size, nullptr);
                    assert(res == 0); boost::ignore_unused(res);
                    lua_rawset(L, -3);

                    lua_pushliteral(L, "buffered_line");
                    res = luaL_loadbuffer(
                        L,
                        reinterpret_cast<char*>(scanner_buffered_line_bytecode),
                        scanner_buffered_line_bytecode_size, nullptr);
                    assert(res == 0); boost::ignore_unused(res);
                    lua_rawset(L, -3);
                }
                lua_rawset(L, -3);
            }
            lua_pushliteral(L, "mt");
            lua_pushvalue(L, -2);
            lua_rawset(L, -4);

            lua_pushliteral(L, "new");
            res = luaL_loadbuffer(
                L, reinterpret_cast<char*>(scanner_new_bytecode),
                scanner_new_bytecode_size, nullptr);
            assert(res == 0); boost::ignore_unused(res);
            lua_pushvalue(L, -3);
            rawgetp(L, LUA_REGISTRYINDEX, &raw_setmetatable_key);
            lua_pushcfunction(L, byte_span_new);
            lua_call(L, 3, 1);
            lua_rawset(L, -4);

            lua_pushliteral(L, "with_awk_defaults");
            res = luaL_loadbuffer(
                L, reinterpret_cast<char*>(scanner_with_awk_defaults_bytecode),
                scanner_with_awk_defaults_bytecode_size, nullptr);
            assert(res == 0); boost::ignore_unused(res);
            lua_pushvalue(L, -3);
            rawgetp(L, LUA_REGISTRYINDEX, &raw_setmetatable_key);
            lua_pushcfunction(L, byte_span_new);
            lua_pushcfunction(L, regex_new);
            lua_call(L, 4, 1);
            lua_rawset(L, -4);

            lua_pop(L, 1);
        }
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);
}

} // namespace emilua
