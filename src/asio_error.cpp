/* Copyright (c) 2023 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#include <emilua/asio_error.hpp>

#include <emilua/dispatch_table.hpp>

namespace emilua {

char asio_error_key;

static int basic_mt_index(lua_State* L)
{
    using error = asio::error::basic_errors;
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("access_denied"),
                [](lua_State* L) -> int {
                    push(L, make_error_code(error::access_denied));
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("address_family_not_supported"),
                [](lua_State* L) -> int {
                    push(
                        L,
                        make_error_code(error::address_family_not_supported));
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("address_in_use"),
                [](lua_State* L) -> int {
                    push(L, make_error_code(error::address_in_use));
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("already_connected"),
                [](lua_State* L) -> int {
                    push(L, make_error_code(error::already_connected));
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("already_started"),
                [](lua_State* L) -> int {
                    push(L, make_error_code(error::already_started));
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("broken_pipe"),
                [](lua_State* L) -> int {
                    push(L, make_error_code(error::broken_pipe));
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("connection_aborted"),
                [](lua_State* L) -> int {
                    push(L, make_error_code(error::connection_aborted));
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("connection_refused"),
                [](lua_State* L) -> int {
                    push(L, make_error_code(error::connection_refused));
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("connection_reset"),
                [](lua_State* L) -> int {
                    push(L, make_error_code(error::connection_reset));
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("bad_descriptor"),
                [](lua_State* L) -> int {
                    push(L, make_error_code(error::bad_descriptor));
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("fault"),
                [](lua_State* L) -> int {
                    push(L, make_error_code(error::fault));
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("host_unreachable"),
                [](lua_State* L) -> int {
                    push(L, make_error_code(error::host_unreachable));
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("in_progress"),
                [](lua_State* L) -> int {
                    push(L, make_error_code(error::in_progress));
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("interrupted"),
                [](lua_State* L) -> int {
                    push(L, make_error_code(error::interrupted));
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("invalid_argument"),
                [](lua_State* L) -> int {
                    push(L, make_error_code(error::invalid_argument));
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("message_size"),
                [](lua_State* L) -> int {
                    push(L, make_error_code(error::message_size));
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("name_too_long"),
                [](lua_State* L) -> int {
                    push(L, make_error_code(error::name_too_long));
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("network_down"),
                [](lua_State* L) -> int {
                    push(L, make_error_code(error::network_down));
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("network_reset"),
                [](lua_State* L) -> int {
                    push(L, make_error_code(error::network_reset));
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("network_unreachable"),
                [](lua_State* L) -> int {
                    push(L, make_error_code(error::network_unreachable));
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("no_descriptors"),
                [](lua_State* L) -> int {
                    push(L, make_error_code(error::no_descriptors));
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("no_buffer_space"),
                [](lua_State* L) -> int {
                    push(L, make_error_code(error::no_buffer_space));
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("no_memory"),
                [](lua_State* L) -> int {
                    push(L, make_error_code(error::no_memory));
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("no_permission"),
                [](lua_State* L) -> int {
                    push(L, make_error_code(error::no_permission));
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("no_protocol_option"),
                [](lua_State* L) -> int {
                    push(L, make_error_code(error::no_protocol_option));
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("no_such_device"),
                [](lua_State* L) -> int {
                    push(L, make_error_code(error::no_such_device));
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("not_connected"),
                [](lua_State* L) -> int {
                    push(L, make_error_code(error::not_connected));
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("not_socket"),
                [](lua_State* L) -> int {
                    push(L, make_error_code(error::not_socket));
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("operation_aborted"),
                [](lua_State* L) -> int {
                    push(L, make_error_code(error::operation_aborted));
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("operation_not_supported"),
                [](lua_State* L) -> int {
                    push(L, make_error_code(error::operation_not_supported));
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("shut_down"),
                [](lua_State* L) -> int {
                    push(L, make_error_code(error::shut_down));
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("timed_out"),
                [](lua_State* L) -> int {
                    push(L, make_error_code(error::timed_out));
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("try_again"),
                [](lua_State* L) -> int {
                    push(L, make_error_code(error::try_again));
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("would_block"),
                [](lua_State* L) -> int {
                    push(L, make_error_code(error::would_block));
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

static int netdb_mt_index(lua_State* L)
{
    using error = asio::error::netdb_errors;
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("host_not_found"),
                [](lua_State* L) -> int {
                    push(L, make_error_code(error::host_not_found));
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("host_not_found_try_again"),
                [](lua_State* L) -> int {
                    push(L, make_error_code(error::host_not_found_try_again));
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("no_data"),
                [](lua_State* L) -> int {
                    push(L, make_error_code(error::no_data));
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("no_recovery"),
                [](lua_State* L) -> int {
                    push(L, make_error_code(error::no_recovery));
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

static int addrinfo_mt_index(lua_State* L)
{
    using error = asio::error::addrinfo_errors;
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("service_not_found"),
                [](lua_State* L) -> int {
                    push(L, make_error_code(error::service_not_found));
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("socket_type_not_supported"),
                [](lua_State* L) -> int {
                    push(L, make_error_code(error::socket_type_not_supported));
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

static int misc_mt_index(lua_State* L)
{
    using error = asio::error::misc_errors;
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("already_open"),
                [](lua_State* L) -> int {
                    push(L, make_error_code(error::already_open));
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("eof"),
                [](lua_State* L) -> int {
                    push(L, make_error_code(error::eof));
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("not_found"),
                [](lua_State* L) -> int {
                    push(L, make_error_code(error::not_found));
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("fd_set_failure"),
                [](lua_State* L) -> int {
                    push(L, make_error_code(error::fd_set_failure));
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
