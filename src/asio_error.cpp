/* Copyright (c) 2023 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

EMILUA_GPERF_DECLS_BEGIN(includes)
#include <emilua/asio_error.hpp>
EMILUA_GPERF_DECLS_END(includes)

namespace emilua {

char asio_error_key;

static int basic_mt_index(lua_State* L)
{
    auto key = tostringview(L, 2);
    int code = EMILUA_GPERF_BEGIN(key)
        EMILUA_GPERF_PARAM(int action)
        EMILUA_GPERF_DEFAULT_VALUE(0)
        EMILUA_GPERF_PAIR(
            "access_denied", asio::error::basic_errors::access_denied)
        EMILUA_GPERF_PAIR(
            "address_family_not_supported",
            asio::error::basic_errors::address_family_not_supported)
        EMILUA_GPERF_PAIR(
            "address_in_use", asio::error::basic_errors::address_in_use)
        EMILUA_GPERF_PAIR(
            "already_connected", asio::error::basic_errors::already_connected)
        EMILUA_GPERF_PAIR(
            "already_started", asio::error::basic_errors::already_started)
        EMILUA_GPERF_PAIR("broken_pipe", asio::error::basic_errors::broken_pipe)
        EMILUA_GPERF_PAIR(
            "connection_aborted", asio::error::basic_errors::connection_aborted)
        EMILUA_GPERF_PAIR(
            "connection_refused", asio::error::basic_errors::connection_refused)
        EMILUA_GPERF_PAIR(
            "connection_reset", asio::error::basic_errors::connection_reset)
        EMILUA_GPERF_PAIR(
            "bad_descriptor", asio::error::basic_errors::bad_descriptor)
        EMILUA_GPERF_PAIR("fault", asio::error::basic_errors::fault)
        EMILUA_GPERF_PAIR(
            "host_unreachable", asio::error::basic_errors::host_unreachable)
        EMILUA_GPERF_PAIR("in_progress", asio::error::basic_errors::in_progress)
        EMILUA_GPERF_PAIR("interrupted", asio::error::basic_errors::interrupted)
        EMILUA_GPERF_PAIR(
            "invalid_argument", asio::error::basic_errors::invalid_argument)
        EMILUA_GPERF_PAIR(
            "message_size", asio::error::basic_errors::message_size)
        EMILUA_GPERF_PAIR(
            "name_too_long", asio::error::basic_errors::name_too_long)
        EMILUA_GPERF_PAIR(
            "network_down", asio::error::basic_errors::network_down)
        EMILUA_GPERF_PAIR(
            "network_reset", asio::error::basic_errors::network_reset)
        EMILUA_GPERF_PAIR(
            "network_unreachable",
            asio::error::basic_errors::network_unreachable)
        EMILUA_GPERF_PAIR(
            "no_descriptors", asio::error::basic_errors::no_descriptors)
        EMILUA_GPERF_PAIR(
            "no_buffer_space", asio::error::basic_errors::no_buffer_space)
        EMILUA_GPERF_PAIR("no_memory", asio::error::basic_errors::no_memory)
        EMILUA_GPERF_PAIR(
            "no_permission", asio::error::basic_errors::no_permission)
        EMILUA_GPERF_PAIR(
            "no_protocol_option", asio::error::basic_errors::no_protocol_option)
        EMILUA_GPERF_PAIR(
            "no_such_device", asio::error::basic_errors::no_such_device)
        EMILUA_GPERF_PAIR(
            "not_connected", asio::error::basic_errors::not_connected)
        EMILUA_GPERF_PAIR("not_socket", asio::error::basic_errors::not_socket)
        EMILUA_GPERF_PAIR(
            "operation_aborted", asio::error::basic_errors::operation_aborted)
        EMILUA_GPERF_PAIR(
            "operation_not_supported",
            asio::error::basic_errors::operation_not_supported)
        EMILUA_GPERF_PAIR("shut_down", asio::error::basic_errors::shut_down)
        EMILUA_GPERF_PAIR("timed_out", asio::error::basic_errors::timed_out)
        EMILUA_GPERF_PAIR("try_again", asio::error::basic_errors::try_again)
        EMILUA_GPERF_PAIR("would_block", asio::error::basic_errors::would_block)
    EMILUA_GPERF_END(key);
    if (code == 0) {
        push(L, errc::bad_index, "index", 2);
        return lua_error(L);
    }

    push(L, make_error_code(static_cast<asio::error::basic_errors>(code)));
    return 1;
}

static int netdb_mt_index(lua_State* L)
{
    auto key = tostringview(L, 2);
    int code = EMILUA_GPERF_BEGIN(key)
        EMILUA_GPERF_PARAM(int action)
        EMILUA_GPERF_DEFAULT_VALUE(0)
        EMILUA_GPERF_PAIR(
            "host_not_found", asio::error::netdb_errors::host_not_found)
        EMILUA_GPERF_PAIR(
            "host_not_found_try_again",
            asio::error::netdb_errors::host_not_found_try_again)
        EMILUA_GPERF_PAIR("no_data", asio::error::netdb_errors::no_data)
        EMILUA_GPERF_PAIR("no_recovery", asio::error::netdb_errors::no_recovery)
    EMILUA_GPERF_END(key);
    if (code == 0) {
        push(L, errc::bad_index, "index", 2);
        return lua_error(L);
    }

    push(L, make_error_code(static_cast<asio::error::netdb_errors>(code)));
    return 1;
}

static int addrinfo_mt_index(lua_State* L)
{
    auto key = tostringview(L, 2);
    int code = EMILUA_GPERF_BEGIN(key)
        EMILUA_GPERF_PARAM(int action)
        EMILUA_GPERF_DEFAULT_VALUE(0)
        EMILUA_GPERF_PAIR(
            "service_not_found",
            asio::error::addrinfo_errors::service_not_found)
        EMILUA_GPERF_PAIR(
            "socket_type_not_supported",
            asio::error::addrinfo_errors::socket_type_not_supported)
    EMILUA_GPERF_END(key);
    if (code == 0) {
        push(L, errc::bad_index, "index", 2);
        return lua_error(L);
    }

    push(L, make_error_code(static_cast<asio::error::addrinfo_errors>(code)));
    return 1;
}

static int misc_mt_index(lua_State* L)
{
    auto key = tostringview(L, 2);
    int code = EMILUA_GPERF_BEGIN(key)
        EMILUA_GPERF_PARAM(int action)
        EMILUA_GPERF_DEFAULT_VALUE(0)
        EMILUA_GPERF_PAIR(
            "already_open", asio::error::misc_errors::already_open)
        EMILUA_GPERF_PAIR("eof", asio::error::misc_errors::eof)
        EMILUA_GPERF_PAIR("not_found", asio::error::misc_errors::not_found)
        EMILUA_GPERF_PAIR(
            "fd_set_failure", asio::error::misc_errors::fd_set_failure)
    EMILUA_GPERF_END(key);
    if (code == 0) {
        push(L, errc::bad_index, "index", 2);
        return lua_error(L);
    }

    push(L, make_error_code(static_cast<asio::error::misc_errors>(code)));
    return 1;
}

void init_asio_error(lua_State* L)
{
    lua_pushlightuserdata(L, &asio_error_key);
    lua_newuserdata(L, sizeof(char));
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/2);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "asio_error");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/4);

            lua_pushliteral(L, "basic");
            lua_newuserdata(L, sizeof(char));
            {
                lua_createtable(L, /*narr=*/0, /*nrec=*/2);

                lua_pushliteral(L, "__metatable");
                lua_pushliteral(L, "asio_error.basic");
                lua_rawset(L, -3);

                lua_pushliteral(L, "__index");
                lua_pushcfunction(L, basic_mt_index);
                lua_rawset(L, -3);
            }
            setmetatable(L, -2);
            lua_rawset(L, -3);

            lua_pushliteral(L, "netdb");
            lua_newuserdata(L, sizeof(char));
            {
                lua_createtable(L, /*narr=*/0, /*nrec=*/2);

                lua_pushliteral(L, "__metatable");
                lua_pushliteral(L, "asio_error.netdb");
                lua_rawset(L, -3);

                lua_pushliteral(L, "__index");
                lua_pushcfunction(L, netdb_mt_index);
                lua_rawset(L, -3);
            }
            setmetatable(L, -2);
            lua_rawset(L, -3);

            lua_pushliteral(L, "addrinfo");
            lua_newuserdata(L, sizeof(char));
            {
                lua_createtable(L, /*narr=*/0, /*nrec=*/2);

                lua_pushliteral(L, "__metatable");
                lua_pushliteral(L, "asio_error.addrinfo");
                lua_rawset(L, -3);

                lua_pushliteral(L, "__index");
                lua_pushcfunction(L, addrinfo_mt_index);
                lua_rawset(L, -3);
            }
            setmetatable(L, -2);
            lua_rawset(L, -3);

            lua_pushliteral(L, "misc");
            lua_newuserdata(L, sizeof(char));
            {
                lua_createtable(L, /*narr=*/0, /*nrec=*/2);

                lua_pushliteral(L, "__metatable");
                lua_pushliteral(L, "asio_error.misc");
                lua_rawset(L, -3);

                lua_pushliteral(L, "__index");
                lua_pushcfunction(L, misc_mt_index);
                lua_rawset(L, -3);
            }
            setmetatable(L, -2);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);
    }
    setmetatable(L, -2);
    lua_rawset(L, LUA_REGISTRYINDEX);
}

} // namespace emilua
