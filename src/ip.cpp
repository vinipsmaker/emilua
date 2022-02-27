/* Copyright (c) 2020 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#include <boost/asio/ip/host_name.hpp>
#include <boost/asio/ip/multicast.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/unicast.hpp>
#include <boost/asio/ip/v6_only.hpp>
#include <boost/predef/os/windows.h>
#include <boost/asio/ip/tcp.hpp>

#include <emilua/dispatch_table.hpp>
#include <emilua/byte_span.hpp>
#include <emilua/ip.hpp>

#if BOOST_OS_WINDOWS && EMILUA_CONFIG_ENABLE_FILE_IO
#include <boost/asio/windows/overlapped_ptr.hpp>
#include <boost/asio/random_access_file.hpp>
#include <emilua/file.hpp>
#endif // BOOST_OS_WINDOWS && EMILUA_CONFIG_ENABLE_FILE_IO

namespace emilua {

extern unsigned char connect_bytecode[];
extern std::size_t connect_bytecode_size;
extern unsigned char data_op_bytecode[];
extern std::size_t data_op_bytecode_size;
extern unsigned char accept_bytecode[];
extern std::size_t accept_bytecode_size;
extern unsigned char resolve_bytecode[];
extern std::size_t resolve_bytecode_size;

char ip_key;
char ip_address_mt_key;
char ip_tcp_socket_mt_key;
char ip_tcp_acceptor_mt_key;
char ip_udp_socket_mt_key;

static char tcp_socket_connect_key;
static char tcp_socket_read_some_key;
static char tcp_socket_write_some_key;
static char tcp_socket_receive_key;
static char tcp_socket_send_key;
static char tcp_socket_wait_key;
static char tcp_acceptor_accept_key;
static char udp_socket_receive_key;
static char udp_socket_send_key;
static char udp_socket_send_to_key;

#if BOOST_OS_WINDOWS && EMILUA_CONFIG_ENABLE_FILE_IO
static char tcp_socket_send_file_key;
#endif // BOOST_OS_WINDOWS && EMILUA_CONFIG_ENABLE_FILE_IO

struct resolver_service: public pending_operation
{
    resolver_service(asio::io_context& ioctx)
        : pending_operation{/*shared_ownership=*/false}
        , tcp_resolver{ioctx}
        , udp_resolver{ioctx}
    {}

    void cancel() noexcept override
    {}

    asio::ip::tcp::resolver tcp_resolver;
    asio::ip::udp::resolver udp_resolver;
};

static int ip_host_name(lua_State* L)
{
    boost::system::error_code ec;
    auto val = asio::ip::host_name(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    push(L, val);
    return 1;
}

static int address_new(lua_State* L)
{
    lua_settop(L, 1);
    auto a = reinterpret_cast<asio::ip::address*>(
        lua_newuserdata(L, sizeof(asio::ip::address))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    setmetatable(L, -2);
    switch (lua_type(L, 1)) {
    case LUA_TNIL:
        new (a) asio::ip::address{};
        break;
    case LUA_TSTRING: {
        boost::system::error_code ec;
        new (a) asio::ip::address{
            asio::ip::make_address(lua_tostring(L, 1), ec)
        };
        if (ec) {
            push(L, static_cast<std::error_code>(ec));
            return lua_error(L);
        }
        break;
    }
    default:
        static_assert(std::is_trivially_destructible_v<asio::ip::address>);
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    return 1;
}

static int address_any_v4(lua_State* L)
{
    auto a = reinterpret_cast<asio::ip::address*>(
        lua_newuserdata(L, sizeof(asio::ip::address))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    setmetatable(L, -2);
    new (a) asio::ip::address{asio::ip::address_v4::any()};
    return 1;
}

static int address_any_v6(lua_State* L)
{
    auto a = reinterpret_cast<asio::ip::address*>(
        lua_newuserdata(L, sizeof(asio::ip::address))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    setmetatable(L, -2);
    new (a) asio::ip::address{asio::ip::address_v6::any()};
    return 1;
}

static int address_loopback_v4(lua_State* L)
{
    auto a = reinterpret_cast<asio::ip::address*>(
        lua_newuserdata(L, sizeof(asio::ip::address))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    setmetatable(L, -2);
    new (a) asio::ip::address{asio::ip::address_v4::loopback()};
    return 1;
}

static int address_loopback_v6(lua_State* L)
{
    auto a = reinterpret_cast<asio::ip::address*>(
        lua_newuserdata(L, sizeof(asio::ip::address))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    setmetatable(L, -2);
    new (a) asio::ip::address{asio::ip::address_v6::loopback()};
    return 1;
}

static int address_broadcast_v4(lua_State* L)
{
    auto a = reinterpret_cast<asio::ip::address*>(
        lua_newuserdata(L, sizeof(asio::ip::address))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    setmetatable(L, -2);
    new (a) asio::ip::address{asio::ip::address_v4::broadcast()};
    return 1;
}

static int address_to_v6(lua_State* L)
{
    auto a = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 1));
    if (!a || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    if (!lua_rawequal(L, -1, -2) || !a->is_v4()) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto ret = reinterpret_cast<asio::ip::address*>(
        lua_newuserdata(L, sizeof(asio::ip::address))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    setmetatable(L, -2);
    new (ret) asio::ip::address{
        asio::ip::make_address_v6(asio::ip::v4_mapped, a->to_v4())
    };

    return 1;
}

static int address_to_v4(lua_State* L)
{
    auto a = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 1));
    if (!a || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    if (!lua_rawequal(L, -1, -2) || !a->is_v6()) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto ret = reinterpret_cast<asio::ip::address*>(
        lua_newuserdata(L, sizeof(asio::ip::address))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    setmetatable(L, -2);
    try {
        new (ret) asio::ip::address{
            asio::ip::make_address_v4(asio::ip::v4_mapped, a->to_v6())
        };
    } catch (const asio::ip::bad_address_cast&) {
        static_assert(std::is_trivially_destructible_v<asio::ip::address>);
        push(L, std::errc::invalid_argument);
        return lua_error(L);
    }

    return 1;
}

inline int address_is_loopback(lua_State* L)
{
    auto a = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 1));
    lua_pushboolean(L, a->is_loopback());
    return 1;
}

inline int address_is_multicast(lua_State* L)
{
    auto a = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 1));
    lua_pushboolean(L, a->is_multicast());
    return 1;
}

inline int address_is_unspecified(lua_State* L)
{
    auto a = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 1));
    lua_pushboolean(L, a->is_unspecified());
    return 1;
}

inline int address_is_v4(lua_State* L)
{
    auto a = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 1));
    lua_pushboolean(L, a->is_v4());
    return 1;
}

inline int address_is_v6(lua_State* L)
{
    auto a = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 1));
    lua_pushboolean(L, a->is_v6());
    return 1;
}

inline int address_is_link_local(lua_State* L)
{
    auto a = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 1));
    if (!a->is_v6()) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    lua_pushboolean(L, a->to_v6().is_link_local());
    return 1;
}

inline int address_is_multicast_global(lua_State* L)
{
    auto a = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 1));
    if (!a->is_v6()) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    lua_pushboolean(L, a->to_v6().is_multicast_global());
    return 1;
}

inline int address_is_multicast_link_local(lua_State* L)
{
    auto a = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 1));
    if (!a->is_v6()) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    lua_pushboolean(L, a->to_v6().is_multicast_link_local());
    return 1;
}

inline int address_is_multicast_node_local(lua_State* L)
{
    auto a = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 1));
    if (!a->is_v6()) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    lua_pushboolean(L, a->to_v6().is_multicast_node_local());
    return 1;
}

inline int address_is_multicast_org_local(lua_State* L)
{
    auto a = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 1));
    if (!a->is_v6()) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    lua_pushboolean(L, a->to_v6().is_multicast_org_local());
    return 1;
}

inline int address_is_multicast_site_local(lua_State* L)
{
    auto a = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 1));
    if (!a->is_v6()) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    lua_pushboolean(L, a->to_v6().is_multicast_site_local());
    return 1;
}

inline int address_is_site_local(lua_State* L)
{
    auto a = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 1));
    if (!a->is_v6()) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    lua_pushboolean(L, a->to_v6().is_site_local());
    return 1;
}

inline int address_is_v4_mapped(lua_State* L)
{
    auto a = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 1));
    if (!a->is_v6()) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    lua_pushboolean(L, a->to_v6().is_v4_mapped());
    return 1;
}

inline int address_scope_id_get(lua_State* L)
{
    auto a = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 1));
    if (!a->is_v6()) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    lua_pushnumber(L, a->to_v6().scope_id());
    return 1;
}

static int address_mt_index(lua_State* L)
{
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("to_v6"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, address_to_v6);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("to_v4"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, address_to_v4);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("is_loopback"), address_is_loopback),
            hana::make_pair(
                BOOST_HANA_STRING("is_multicast"), address_is_multicast),
            hana::make_pair(
                BOOST_HANA_STRING("is_unspecified"), address_is_unspecified),
            hana::make_pair(BOOST_HANA_STRING("is_v4"), address_is_v4),
            hana::make_pair(BOOST_HANA_STRING("is_v6"), address_is_v6),

            // v6-only properties
            hana::make_pair(
                BOOST_HANA_STRING("is_link_local"), address_is_link_local),
            hana::make_pair(
                BOOST_HANA_STRING("is_multicast_global"),
                address_is_multicast_global),
            hana::make_pair(
                BOOST_HANA_STRING("is_multicast_link_local"),
                address_is_multicast_link_local),
            hana::make_pair(
                BOOST_HANA_STRING("is_multicast_node_local"),
                address_is_multicast_node_local),
            hana::make_pair(
                BOOST_HANA_STRING("is_multicast_org_local"),
                address_is_multicast_org_local),
            hana::make_pair(
                BOOST_HANA_STRING("is_multicast_site_local"),
                address_is_multicast_site_local),
            hana::make_pair(
                BOOST_HANA_STRING("is_site_local"), address_is_site_local),
            hana::make_pair(
                BOOST_HANA_STRING("is_v4_mapped"), address_is_v4_mapped),
            hana::make_pair(BOOST_HANA_STRING("scope_id"), address_scope_id_get)
        ),
        [](std::string_view /*key*/, lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        },
        tostringview(L, 2),
        L
    );
}

inline int address_scope_id_set(lua_State* L)
{
    luaL_checktype(L, 3, LUA_TNUMBER);

    auto a = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 1));
    if (!a->is_v6()) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto as_v6 = a->to_v6();
    as_v6.scope_id(lua_tonumber(L, 3));
    *a = as_v6;
    return 0;
}

static int address_mt_newindex(lua_State* L)
{
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(BOOST_HANA_STRING("scope_id"), address_scope_id_set)
        ),
        [](std::string_view /*key*/, lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        },
        tostringview(L, 2),
        L
    );
}

static int address_mt_tostring(lua_State* L)
{
    auto a = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 1));
    auto ret = a->to_string();
    lua_pushlstring(L, ret.data(), ret.size());
    return 1;
}

static int address_mt_eq(lua_State* L)
{
    auto a1 = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 1));
    auto a2 = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 2));
    lua_pushboolean(L, *a1 == *a2);
    return 1;
}

static int address_mt_lt(lua_State* L)
{
    auto a1 = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 1));
    auto a2 = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 2));
    lua_pushboolean(L, *a1 < *a2);
    return 1;
}

static int address_mt_le(lua_State* L)
{
    auto a1 = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 1));
    auto a2 = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 2));
    lua_pushboolean(L, *a1 <= *a2);
    return 1;
}

static int tcp_socket_new(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);
    auto a = reinterpret_cast<tcp_socket*>(
        lua_newuserdata(L, sizeof(tcp_socket))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_socket_mt_key);
    setmetatable(L, -2);
    new (a) tcp_socket{vm_ctx.strand().context()};
    return 1;
}

static int tcp_socket_open(lua_State* L)
{
    lua_settop(L, 2);
    auto sock = reinterpret_cast<tcp_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    switch (lua_type(L, 2)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    case LUA_TUSERDATA: {
        auto addr = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 2));
        if (!addr || !lua_getmetatable(L, 2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        boost::system::error_code ec;
        sock->socket.open(asio::ip::tcp::endpoint{*addr, 0}.protocol(), ec);
        if (ec) {
            push(L, static_cast<std::error_code>(ec));
            return lua_error(L);
        }
        return 0;
    }
    case LUA_TSTRING:
        return dispatch_table::dispatch(
            hana::make_tuple(
                hana::make_pair(
                    BOOST_HANA_STRING("v4"),
                    [&]() -> int {
                        boost::system::error_code ec;
                        sock->socket.open(asio::ip::tcp::v4(), ec);
                        if (ec) {
                            push(L, static_cast<std::error_code>(ec));
                            return lua_error(L);
                        }
                        return 0;
                    }
                ),
                hana::make_pair(
                    BOOST_HANA_STRING("v6"),
                    [&]() -> int {
                        boost::system::error_code ec;
                        sock->socket.open(asio::ip::tcp::v6(), ec);
                        if (ec) {
                            push(L, static_cast<std::error_code>(ec));
                            return lua_error(L);
                        }
                        return 0;
                    }
                )
            ),
            [&](std::string_view /*key*/) -> int {
                push(L, std::errc::invalid_argument, "arg", 2);
                return lua_error(L);
            },
            tostringview(L, 2)
        );
    }
}

static int tcp_socket_bind(lua_State* L)
{
    luaL_checktype(L, 3, LUA_TNUMBER);
    auto sock = reinterpret_cast<tcp_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    switch (lua_type(L, 2)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    case LUA_TUSERDATA: {
        auto addr = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 2));
        if (!addr || !lua_getmetatable(L, 2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        asio::ip::tcp::endpoint ep(*addr, lua_tointeger(L, 3));
        boost::system::error_code ec;
        sock->socket.bind(ep, ec);
        if (ec) {
            push(L, static_cast<std::error_code>(ec));
            return lua_error(L);
        }
        return 0;
    }
    case LUA_TSTRING: {
        boost::system::error_code ec;
        auto addr = asio::ip::make_address(lua_tostring(L, 2), ec);
        if (ec) {
            push(L, static_cast<std::error_code>(ec));
            return lua_error(L);
        }
        asio::ip::tcp::endpoint ep(addr, lua_tointeger(L, 3));
        sock->socket.bind(ep, ec);
        if (ec) {
            push(L, static_cast<std::error_code>(ec));
            return lua_error(L);
        }
        return 0;
    }
    }
}

static int tcp_socket_close(lua_State* L)
{
    auto sock = reinterpret_cast<tcp_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    sock->socket.close(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int tcp_socket_cancel(lua_State* L)
{
    auto sock = reinterpret_cast<tcp_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    sock->socket.cancel(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int tcp_socket_io_control(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);

    auto socket = reinterpret_cast<tcp_socket*>(
        lua_touserdata(L, 1));
    if (!socket || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("bytes_readable"),
                [&]() -> int {
                    asio::socket_base::bytes_readable command;
                    socket->socket.io_control(command, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    lua_pushnumber(L, command.get());
                    return 1;
                }
            )
        ),
        [L](std::string_view /*key*/) -> int {
            push(L, std::errc::not_supported);
            return lua_error(L);
        },
        tostringview(L, 2)
    );
    return 0;
}

static int tcp_socket_shutdown(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);

    auto socket = reinterpret_cast<tcp_socket*>(
        lua_touserdata(L, 1));
    if (!socket || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    std::error_code ec;
    asio::ip::tcp::socket::shutdown_type what;
    dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("receive"),
                [&]() { what = asio::ip::tcp::socket::shutdown_receive; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("send"),
                [&]() { what = asio::ip::tcp::socket::shutdown_send; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("both"),
                [&]() { what = asio::ip::tcp::socket::shutdown_both; }
            )
        ),
        [&](std::string_view /*key*/) {
            ec = make_error_code(std::errc::invalid_argument);
        },
        tostringview(L, 2)
    );
    if (ec) {
        push(L, ec);
        return lua_error(L);
    }
    boost::system::error_code ec2;
    socket->socket.shutdown(what, ec2);
    if (ec2) {
        push(L, static_cast<std::error_code>(ec2));
        return lua_error(L);
    }
    return 0;
}

static int tcp_socket_connect(lua_State* L)
{
    luaL_checktype(L, 3, LUA_TNUMBER);
    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto s = reinterpret_cast<tcp_socket*>(lua_touserdata(L, 1));
    if (!s || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto a = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 2));
    if (!a || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    asio::ip::tcp::endpoint ep{
        *a, static_cast<std::uint16_t>(lua_tointeger(L, 3))};

    lua_pushvalue(L, 1);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto s = reinterpret_cast<tcp_socket*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            boost::system::error_code ignored_ec;
            s->socket.cancel(ignored_ec);
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    ++s->nbusy;
    s->socket.async_connect(ep, asio::bind_executor(
        vm_ctx->strand_using_defer(),
        [vm_ctx,current_fiber,s](const boost::system::error_code& ec) {
            if (!vm_ctx->valid())
                return;

            --s->nbusy;

            auto opt_args = vm_context::options::arguments;
            vm_ctx->fiber_resume(
                current_fiber,
                hana::make_set(
                    vm_context::options::auto_detect_interrupt,
                    hana::make_pair(opt_args, hana::make_tuple(ec))));
        }
    ));

    return lua_yield(L, 0);
}

static int tcp_socket_read_some(lua_State* L)
{
    lua_settop(L, 2);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto s = reinterpret_cast<tcp_socket*>(lua_touserdata(L, 1));
    if (!s || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto bs = reinterpret_cast<byte_span_handle*>(lua_touserdata(L, 2));
    if (!bs || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &byte_span_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    lua_pushvalue(L, 1);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto s = reinterpret_cast<tcp_socket*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            boost::system::error_code ignored_ec;
            s->socket.cancel(ignored_ec);
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    ++s->nbusy;
    s->socket.async_read_some(
        asio::buffer(bs->data.get(), bs->size),
        asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,buf=bs->data,s](
                const boost::system::error_code& ec,
                std::size_t bytes_transferred
            ) {
                if (!vm_ctx->valid())
                    return;

                --s->nbusy;

                boost::ignore_unused(buf);
                auto opt_args = vm_context::options::arguments;
                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            opt_args,
                            hana::make_tuple(ec, bytes_transferred))));
            }
        )
    );

    return lua_yield(L, 0);
}

static int tcp_socket_write_some(lua_State* L)
{
    lua_settop(L, 2);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto s = reinterpret_cast<tcp_socket*>(lua_touserdata(L, 1));
    if (!s || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto bs = reinterpret_cast<byte_span_handle*>(lua_touserdata(L, 2));
    if (!bs || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &byte_span_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    lua_pushvalue(L, 1);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto s = reinterpret_cast<tcp_socket*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            boost::system::error_code ignored_ec;
            s->socket.cancel(ignored_ec);
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    ++s->nbusy;
    s->socket.async_write_some(
        asio::buffer(bs->data.get(), bs->size),
        asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,buf=bs->data,s](
                const boost::system::error_code& ec,
                std::size_t bytes_transferred
            ) {
                if (!vm_ctx->valid())
                    return;

                --s->nbusy;

                boost::ignore_unused(buf);
                auto opt_args = vm_context::options::arguments;
                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            opt_args,
                            hana::make_tuple(ec, bytes_transferred))));
            }
        )
    );

    return lua_yield(L, 0);
}

static int tcp_socket_receive(lua_State* L)
{
    luaL_checktype(L, 3, LUA_TNUMBER);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto s = reinterpret_cast<tcp_socket*>(lua_touserdata(L, 1));
    if (!s || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto bs = reinterpret_cast<byte_span_handle*>(lua_touserdata(L, 2));
    if (!bs || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &byte_span_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    lua_pushvalue(L, 1);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto s = reinterpret_cast<tcp_socket*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            boost::system::error_code ignored_ec;
            s->socket.cancel(ignored_ec);
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    ++s->nbusy;
    s->socket.async_receive(
        asio::buffer(bs->data.get(), bs->size),
        lua_tointeger(L, 3),
        asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,buf=bs->data,s](
                const boost::system::error_code& ec,
                std::size_t bytes_transferred
            ) {
                if (!vm_ctx->valid())
                    return;

                --s->nbusy;

                boost::ignore_unused(buf);
                auto opt_args = vm_context::options::arguments;
                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            opt_args,
                            hana::make_tuple(ec, bytes_transferred))));
            }
        )
    );

    return lua_yield(L, 0);
}

static int tcp_socket_send(lua_State* L)
{
    luaL_checktype(L, 3, LUA_TNUMBER);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto s = reinterpret_cast<tcp_socket*>(lua_touserdata(L, 1));
    if (!s || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto bs = reinterpret_cast<byte_span_handle*>(lua_touserdata(L, 2));
    if (!bs || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &byte_span_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    lua_pushvalue(L, 1);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto s = reinterpret_cast<tcp_socket*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            boost::system::error_code ignored_ec;
            s->socket.cancel(ignored_ec);
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    ++s->nbusy;
    s->socket.async_send(
        asio::buffer(bs->data.get(), bs->size),
        lua_tointeger(L, 3),
        asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,buf=bs->data,s](
                const boost::system::error_code& ec,
                std::size_t bytes_transferred
            ) {
                if (!vm_ctx->valid())
                    return;

                --s->nbusy;

                boost::ignore_unused(buf);
                auto opt_args = vm_context::options::arguments;
                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            opt_args,
                            hana::make_tuple(ec, bytes_transferred))));
            }
        )
    );

    return lua_yield(L, 0);
}

#if BOOST_OS_WINDOWS && EMILUA_CONFIG_ENABLE_FILE_IO
// https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/example/cpp03/windows/transmit_file.cpp
static int tcp_socket_send_file(lua_State* L)
{
    lua_settop(L, 7);
    luaL_checktype(L, 3, LUA_TNUMBER);
    luaL_checktype(L, 4, LUA_TNUMBER);
    luaL_checktype(L, 5, LUA_TNUMBER);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto sock = reinterpret_cast<tcp_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto file = reinterpret_cast<asio::random_access_file*>(
        lua_touserdata(L, 2));
    if (!file || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_random_access_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    TRANSMIT_FILE_BUFFERS transmitBuffers;
    std::shared_ptr<unsigned char[]> buf1, buf2;

    if (lua_type(L, 6) != LUA_TNIL) {
        auto bs = reinterpret_cast<byte_span_handle*>(lua_touserdata(L, 6));
        if (!bs || !lua_getmetatable(L, 6)) {
            push(L, std::errc::invalid_argument, "arg", 6);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &byte_span_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 6);
            return lua_error(L);
        }
        buf1 = bs->data;
        transmitBuffers.Head = bs->data.get();
        transmitBuffers.HeadLength = bs->size;
    } else {
        transmitBuffers.Head = NULL;
        transmitBuffers.HeadLength = 0;
    }

    if (lua_type(L, 7) != LUA_TNIL) {
        auto bs = reinterpret_cast<byte_span_handle*>(lua_touserdata(L, 7));
        if (!bs || !lua_getmetatable(L, 7)) {
            push(L, std::errc::invalid_argument, "arg", 7);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &byte_span_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 7);
            return lua_error(L);
        }
        buf2 = bs->data;
        transmitBuffers.Tail = bs->data.get();
        transmitBuffers.TailLength = bs->size;
    } else {
        transmitBuffers.Tail = NULL;
        transmitBuffers.TailLength = 0;
    }

    asio::windows::overlapped_ptr overlapped{
        vm_ctx->strand_using_defer(),
        [vm_ctx,current_fiber,buf1,buf2,sock](
            const boost::system::error_code& ec,
            std::size_t bytes_transferred
        ) {
            if (!vm_ctx->valid())
                return;

            --sock->nbusy;

            vm_ctx->fiber_resume(
                current_fiber,
                hana::make_set(
                    vm_context::options::auto_detect_interrupt,
                    hana::make_pair(
                        vm_context::options::arguments,
                        hana::make_tuple(ec, bytes_transferred))));
        }
    };

    DWORD64 offset = lua_tointeger(L, 3);
    overlapped.get()->Offset = static_cast<DWORD>(offset);
    overlapped.get()->OffsetHigh = static_cast<DWORD>(offset >> 32);

    lua_pushvalue(L, 1);
    lua_pushlightuserdata(L, overlapped.get());
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto sock = reinterpret_cast<tcp_socket*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            CancelIoEx(
                reinterpret_cast<HANDLE>(
                    static_cast<SOCKET>(sock->socket.native_handle())),
                reinterpret_cast<LPOVERLAPPED>(
                    lua_touserdata(L, lua_upvalueindex(2))));
            return 0;
        },
        2);
    set_interrupter(L, *vm_ctx);

    ++sock->nbusy;
    BOOL ok = TransmitFile(sock->socket.native_handle(), file->native_handle(),
                           /*nNumberOfBytesToWrite=*/lua_tointeger(L, 4),
                           /*nNumberOfBytesPerSend=*/lua_tointeger(L, 5),
                           overlapped.get(), &transmitBuffers,
                           /*dwReserved=*/0);
    DWORD last_error = GetLastError();

    // Check if the operation completed immediately.
    if (!ok && last_error != ERROR_IO_PENDING) {
        // The operation completed immediately, so a completion notification
        // needs to be posted. When complete() is called, ownership of the
        // OVERLAPPED-derived object passes to the io_context.
        boost::system::error_code ec(last_error,
                                     asio::error::get_system_category());
        overlapped.complete(ec, 0);
    } else {
        // The operation was successfully initiated, so ownership of the
        // OVERLAPPED-derived object has passed to the io_context.
        overlapped.release();
    }

    return lua_yield(L, 0);
}
#endif // BOOST_OS_WINDOWS && EMILUA_CONFIG_ENABLE_FILE_IO

static int tcp_socket_wait(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto s = reinterpret_cast<tcp_socket*>(lua_touserdata(L, 1));
    if (!s || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    std::error_code ec;
    asio::ip::tcp::socket::wait_type wait_type;
    dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("read"),
                [&]() { wait_type = asio::ip::tcp::socket::wait_read; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("write"),
                [&]() { wait_type = asio::ip::tcp::socket::wait_write; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("error"),
                [&]() { wait_type = asio::ip::tcp::socket::wait_error; }
            )
        ),
        [&](std::string_view /*key*/) {
            ec = make_error_code(std::errc::invalid_argument);
        },
        tostringview(L, 2)
    );
    if (ec) {
        push(L, ec);
        return lua_error(L);
    }

    lua_pushvalue(L, 1);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto s = reinterpret_cast<tcp_socket*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            boost::system::error_code ignored_ec;
            s->socket.cancel(ignored_ec);
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    ++s->nbusy;
    s->socket.async_wait(
        wait_type,
        asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,s](const boost::system::error_code& ec) {
                if (!vm_ctx->valid())
                    return;

                --s->nbusy;

                auto opt_args = vm_context::options::arguments;
                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            opt_args, hana::make_tuple(ec))));
            }
        )
    );

    return lua_yield(L, 0);
}

static int tcp_socket_set_option(lua_State* L)
{
    lua_settop(L, 4);
    luaL_checktype(L, 2, LUA_TSTRING);

    auto socket = reinterpret_cast<tcp_socket*>(lua_touserdata(L, 1));
    if (!socket || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("tcp_no_delay"),
                [&]() -> int {
                    luaL_checktype(L, 3, LUA_TBOOLEAN);
                    asio::ip::tcp::no_delay o(lua_toboolean(L, 3));
                    socket->socket.set_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("send_low_watermark"),
                [&]() -> int {
                    luaL_checktype(L, 3, LUA_TNUMBER);
                    asio::socket_base::send_low_watermark o(
                        lua_tointeger(L, 3));
                    socket->socket.set_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("send_buffer_size"),
                [&]() -> int {
                    luaL_checktype(L, 3, LUA_TNUMBER);
                    asio::socket_base::send_buffer_size o(lua_tointeger(L, 3));
                    socket->socket.set_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("receive_low_watermark"),
                [&]() -> int {
                    luaL_checktype(L, 3, LUA_TNUMBER);
                    asio::socket_base::receive_low_watermark o(
                        lua_tointeger(L, 3));
                    socket->socket.set_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("receive_buffer_size"),
                [&]() -> int {
                    luaL_checktype(L, 3, LUA_TNUMBER);
                    asio::socket_base::receive_buffer_size o(
                        lua_tointeger(L, 3));
                    socket->socket.set_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("out_of_band_inline"),
                [&]() -> int {
                    luaL_checktype(L, 3, LUA_TBOOLEAN);
                    asio::socket_base::out_of_band_inline o(
                        lua_toboolean(L, 3));
                    socket->socket.set_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("linger"),
                [&]() -> int {
                    luaL_checktype(L, 3, LUA_TBOOLEAN);
                    luaL_checktype(L, 4, LUA_TNUMBER);
                    asio::socket_base::linger o(
                        lua_toboolean(L, 3), lua_tointeger(L, 4));
                    socket->socket.set_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("keep_alive"),
                [&]() -> int {
                    luaL_checktype(L, 3, LUA_TBOOLEAN);
                    asio::socket_base::keep_alive o(lua_toboolean(L, 3));
                    socket->socket.set_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("do_not_route"),
                [&]() -> int {
                    luaL_checktype(L, 3, LUA_TBOOLEAN);
                    asio::socket_base::do_not_route o(lua_toboolean(L, 3));
                    socket->socket.set_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("debug"),
                [&]() -> int {
                    luaL_checktype(L, 3, LUA_TBOOLEAN);
                    asio::socket_base::debug o(lua_toboolean(L, 3));
                    socket->socket.set_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("v6_only"),
                [&]() -> int {
                    luaL_checktype(L, 3, LUA_TBOOLEAN);
                    asio::ip::v6_only o(lua_toboolean(L, 3));
                    socket->socket.set_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            )
        ),
        [L](std::string_view /*key*/) -> int {
            push(L, std::errc::not_supported);
            return lua_error(L);
        },
        tostringview(L, 2)
    );
}

static int tcp_socket_get_option(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);

    auto socket = reinterpret_cast<tcp_socket*>(lua_touserdata(L, 1));
    if (!socket || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("tcp_no_delay"),
                [&]() -> int {
                    asio::ip::tcp::no_delay o;
                    socket->socket.get_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    lua_pushboolean(L, o.value());
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("send_low_watermark"),
                [&]() -> int {
                    asio::socket_base::send_low_watermark o;
                    socket->socket.get_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    lua_pushinteger(L, o.value());
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("send_buffer_size"),
                [&]() -> int {
                    asio::socket_base::send_buffer_size o;
                    socket->socket.get_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    lua_pushinteger(L, o.value());
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("receive_low_watermark"),
                [&]() -> int {
                    asio::socket_base::receive_low_watermark o;
                    socket->socket.get_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    lua_pushinteger(L, o.value());
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("receive_buffer_size"),
                [&]() -> int {
                    asio::socket_base::receive_buffer_size o;
                    socket->socket.get_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    lua_pushinteger(L, o.value());
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("out_of_band_inline"),
                [&]() -> int {
                    asio::socket_base::out_of_band_inline o;
                    socket->socket.get_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    lua_pushboolean(L, o.value());
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("linger"),
                [&]() -> int {
                    asio::socket_base::linger o;
                    socket->socket.get_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    lua_pushboolean(L, o.enabled());
                    lua_pushinteger(L, o.timeout());
                    return 2;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("keep_alive"),
                [&]() -> int {
                    asio::socket_base::keep_alive o;
                    socket->socket.get_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    lua_pushboolean(L, o.value());
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("do_not_route"),
                [&]() -> int {
                    asio::socket_base::do_not_route o;
                    socket->socket.get_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    lua_pushboolean(L, o.value());
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("debug"),
                [&]() -> int {
                    asio::socket_base::debug o;
                    socket->socket.get_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    lua_pushboolean(L, o.value());
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("v6_only"),
                [&]() -> int {
                    asio::ip::v6_only o;
                    socket->socket.get_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    lua_pushboolean(L, o.value());
                    return 1;
                }
            )
        ),
        [L](std::string_view /*key*/) -> int {
            push(L, std::errc::not_supported);
            return lua_error(L);
        },
        tostringview(L, 2)
    );
}

inline int tcp_socket_is_open(lua_State* L)
{
    auto sock = reinterpret_cast<tcp_socket*>(lua_touserdata(L, 1));
    lua_pushboolean(L, sock->socket.is_open());
    return 1;
}

inline int tcp_socket_local_address(lua_State* L)
{
    auto sock = reinterpret_cast<tcp_socket*>(lua_touserdata(L, 1));
    boost::system::error_code ec;
    auto ep = sock->socket.local_endpoint(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    auto addr = reinterpret_cast<asio::ip::address*>(
        lua_newuserdata(L, sizeof(asio::ip::address))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    setmetatable(L, -2);
    new (addr) asio::ip::address{ep.address()};
    return 1;
}

inline int tcp_socket_local_port(lua_State* L)
{
    auto sock = reinterpret_cast<tcp_socket*>(lua_touserdata(L, 1));
    boost::system::error_code ec;
    auto ep = sock->socket.local_endpoint(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    lua_pushinteger(L, ep.port());
    return 1;
}

inline int tcp_socket_remote_address(lua_State* L)
{
    auto sock = reinterpret_cast<tcp_socket*>(lua_touserdata(L, 1));
    boost::system::error_code ec;
    auto ep = sock->socket.remote_endpoint(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    auto addr = reinterpret_cast<asio::ip::address*>(
        lua_newuserdata(L, sizeof(asio::ip::address))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    setmetatable(L, -2);
    new (addr) asio::ip::address{ep.address()};
    return 1;
}

inline int tcp_socket_remote_port(lua_State* L)
{
    auto sock = reinterpret_cast<tcp_socket*>(lua_touserdata(L, 1));
    boost::system::error_code ec;
    auto ep = sock->socket.remote_endpoint(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    lua_pushinteger(L, ep.port());
    return 1;
}

inline int tcp_socket_at_mark(lua_State* L)
{
    auto socket = reinterpret_cast<tcp_socket*>(lua_touserdata(L, 1));
    boost::system::error_code ec;
    bool ret = socket->socket.at_mark(ec);
    if (ec) {
        push(L, ec);
        return lua_error(L);
    }
    lua_pushboolean(L, ret);
    return 1;
}

static int tcp_socket_mt_index(lua_State* L)
{
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("open"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, tcp_socket_open);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("bind"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, tcp_socket_bind);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("close"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, tcp_socket_close);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("cancel"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, tcp_socket_cancel);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("io_control"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, tcp_socket_io_control);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("shutdown"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, tcp_socket_shutdown);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("connect"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX, &tcp_socket_connect_key);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("read_some"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX, &tcp_socket_read_some_key);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("write_some"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX, &tcp_socket_write_some_key);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("receive"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX, &tcp_socket_receive_key);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("send"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX, &tcp_socket_send_key);
                    return 1;
                }
            ),
#if BOOST_OS_WINDOWS && EMILUA_CONFIG_ENABLE_FILE_IO
            hana::make_pair(
                BOOST_HANA_STRING("send_file"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX, &tcp_socket_send_file_key);
                    return 1;
                }
            ),
#endif // BOOST_OS_WINDOWS && EMILUA_CONFIG_ENABLE_FILE_IO
            hana::make_pair(
                BOOST_HANA_STRING("wait"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX, &tcp_socket_wait_key);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("set_option"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, tcp_socket_set_option);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("get_option"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, tcp_socket_get_option);
                    return 1;
                }
            ),
            hana::make_pair(BOOST_HANA_STRING("is_open"), tcp_socket_is_open),
            hana::make_pair(
                BOOST_HANA_STRING("local_address"), tcp_socket_local_address),
            hana::make_pair(
                BOOST_HANA_STRING("local_port"), tcp_socket_local_port),
            hana::make_pair(
                BOOST_HANA_STRING("remote_address"), tcp_socket_remote_address),
            hana::make_pair(
                BOOST_HANA_STRING("remote_port"), tcp_socket_remote_port),
            hana::make_pair(BOOST_HANA_STRING("at_mark"), tcp_socket_at_mark)
        ),
        [](std::string_view /*key*/, lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        },
        tostringview(L, 2),
        L
    );
}

static int tcp_acceptor_new(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);
    auto a = reinterpret_cast<asio::ip::tcp::acceptor*>(
        lua_newuserdata(L, sizeof(asio::ip::tcp::acceptor))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_acceptor_mt_key);
    setmetatable(L, -2);
    new (a) asio::ip::tcp::acceptor{vm_ctx.strand().context()};
    return 1;
}

static int tcp_acceptor_accept(lua_State* L)
{
    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto acceptor = reinterpret_cast<asio::ip::tcp::acceptor*>(
        lua_touserdata(L, 1));
    if (!acceptor || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_acceptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    lua_pushvalue(L, 1);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto a = reinterpret_cast<asio::ip::tcp::acceptor*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            boost::system::error_code ignored_ec;
            a->cancel(ignored_ec);
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    acceptor->async_accept(asio::bind_executor(
        vm_ctx->strand_using_defer(),
        [vm_ctx,current_fiber](const boost::system::error_code& ec,
                               asio::ip::tcp::socket peer) {
            vm_ctx->fiber_resume(
                current_fiber,
                hana::make_set(
                    vm_context::options::auto_detect_interrupt,
                    hana::make_pair(
                        vm_context::options::arguments,
                        hana::make_tuple(
                            ec,
                            [&ec,&peer](lua_State* fiber) {
                                if (ec) {
                                    lua_pushnil(fiber);
                                } else {
                                    auto s = reinterpret_cast<tcp_socket*>(
                                        lua_newuserdata(
                                            fiber, sizeof(tcp_socket)));
                                    rawgetp(fiber, LUA_REGISTRYINDEX,
                                            &ip_tcp_socket_mt_key);
                                    setmetatable(fiber, -2);
                                    new (s) tcp_socket{std::move(peer)};
                                }
                            }))
                ));
        }
    ));

    return lua_yield(L, 0);
}

static int tcp_acceptor_listen(lua_State* L)
{
    lua_settop(L, 2);
    auto acceptor = reinterpret_cast<asio::ip::tcp::acceptor*>(
        lua_touserdata(L, 1));
    if (!acceptor || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_acceptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    switch (lua_type(L, 2)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    case LUA_TNIL: {
        boost::system::error_code ec;
        acceptor->listen(asio::socket_base::max_listen_connections, ec);
        if (ec) {
            push(L, static_cast<std::error_code>(ec));
            return lua_error(L);
        }
        return 0;
    }
    case LUA_TNUMBER: {
        boost::system::error_code ec;
        acceptor->listen(lua_tointeger(L, 2), ec);
        if (ec) {
            push(L, static_cast<std::error_code>(ec));
            return lua_error(L);
        }
        return 0;
    }
    }
}

static int tcp_acceptor_bind(lua_State* L)
{
    luaL_checktype(L, 3, LUA_TNUMBER);
    auto acceptor = reinterpret_cast<asio::ip::tcp::acceptor*>(
        lua_touserdata(L, 1));
    if (!acceptor || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_acceptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    switch (lua_type(L, 2)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    case LUA_TUSERDATA: {
        auto addr = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 2));
        if (!addr || !lua_getmetatable(L, 2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        boost::system::error_code ec;
        acceptor->bind(asio::ip::tcp::endpoint(*addr, lua_tointeger(L, 3)), ec);
        if (ec) {
            push(L, static_cast<std::error_code>(ec));
            return lua_error(L);
        }
        return 0;
    }
    case LUA_TSTRING: {
        boost::system::error_code ec;
        auto addr = asio::ip::make_address(lua_tostring(L, 2), ec);
        if (ec) {
            push(L, static_cast<std::error_code>(ec));
            return lua_error(L);
        }
        acceptor->bind(asio::ip::tcp::endpoint(addr, lua_tointeger(L, 3)), ec);
        if (ec) {
            push(L, static_cast<std::error_code>(ec));
            return lua_error(L);
        }
        return 0;
    }
    }
}

static int tcp_acceptor_open(lua_State* L)
{
    lua_settop(L, 2);
    auto acceptor = reinterpret_cast<asio::ip::tcp::acceptor*>(
        lua_touserdata(L, 1));
    if (!acceptor || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_acceptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    switch (lua_type(L, 2)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    case LUA_TUSERDATA: {
        auto addr = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 2));
        if (!addr || !lua_getmetatable(L, 2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        boost::system::error_code ec;
        acceptor->open(asio::ip::tcp::endpoint{*addr, 0}.protocol(), ec);
        if (ec) {
            push(L, static_cast<std::error_code>(ec));
            return lua_error(L);
        }
        return 0;
    }
    case LUA_TSTRING:
        return dispatch_table::dispatch(
            hana::make_tuple(
                hana::make_pair(
                    BOOST_HANA_STRING("v4"),
                    [&]() -> int {
                        boost::system::error_code ec;
                        acceptor->open(asio::ip::tcp::v4(), ec);
                        if (ec) {
                            push(L, static_cast<std::error_code>(ec));
                            return lua_error(L);
                        }
                        return 0;
                    }
                ),
                hana::make_pair(
                    BOOST_HANA_STRING("v6"),
                    [&]() -> int {
                        boost::system::error_code ec;
                        acceptor->open(asio::ip::tcp::v6(), ec);
                        if (ec) {
                            push(L, static_cast<std::error_code>(ec));
                            return lua_error(L);
                        }
                        return 0;
                    }
                )
            ),
            [&](std::string_view /*key*/) -> int {
                push(L, std::errc::invalid_argument, "arg", 2);
                return lua_error(L);
            },
            tostringview(L, 2)
        );
    }
}

static int tcp_acceptor_close(lua_State* L)
{
    auto acceptor = reinterpret_cast<asio::ip::tcp::acceptor*>(
        lua_touserdata(L, 1));
    if (!acceptor || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_acceptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    acceptor->close(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int tcp_acceptor_set_option(lua_State* L)
{
    lua_settop(L, 3);
    luaL_checktype(L, 2, LUA_TSTRING);

    auto acceptor = reinterpret_cast<asio::ip::tcp::acceptor*>(
        lua_touserdata(L, 1));
    if (!acceptor || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_acceptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("debug"),
                [&]() -> int {
                    luaL_checktype(L, 3, LUA_TBOOLEAN);
                    asio::socket_base::debug o(lua_toboolean(L, 3));
                    acceptor->set_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("reuse_address"),
                [&]() -> int {
                    luaL_checktype(L, 3, LUA_TBOOLEAN);
                    asio::socket_base::reuse_address o(lua_toboolean(L, 3));
                    acceptor->set_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("enable_connection_aborted"),
                [&]() -> int {
                    luaL_checktype(L, 3, LUA_TBOOLEAN);
                    asio::socket_base::enable_connection_aborted o(
                        lua_toboolean(L, 3)
                    );
                    acceptor->set_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("v6_only"),
                [&]() -> int {
                    luaL_checktype(L, 3, LUA_TBOOLEAN);
                    asio::ip::v6_only o(lua_toboolean(L, 3));
                    acceptor->set_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            )
        ),
        [L](std::string_view /*key*/) -> int {
            push(L, std::errc::not_supported);
            return lua_error(L);
        },
        tostringview(L, 2)
    );
}

static int tcp_acceptor_get_option(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);

    auto acceptor = reinterpret_cast<asio::ip::tcp::acceptor*>(
        lua_touserdata(L, 1));
    if (!acceptor || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_acceptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("debug"),
                [&]() -> int {
                    asio::socket_base::debug o;
                    acceptor->get_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    lua_pushboolean(L, o.value());
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("reuse_address"),
                [&]() -> int {
                    asio::socket_base::reuse_address o;
                    acceptor->get_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    lua_pushboolean(L, o.value());
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("enable_connection_aborted"),
                [&]() -> int {
                    asio::socket_base::enable_connection_aborted o;
                    acceptor->get_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    lua_pushboolean(L, o.value());
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("v6_only"),
                [&]() -> int {
                    asio::ip::v6_only o;
                    acceptor->get_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    lua_pushboolean(L, o.value());
                    return 1;
                }
            )
        ),
        [L](std::string_view /*key*/) -> int {
            push(L, std::errc::not_supported);
            return lua_error(L);
        },
        tostringview(L, 2)
    );
}

static int tcp_acceptor_cancel(lua_State* L)
{
    auto acceptor = reinterpret_cast<asio::ip::tcp::acceptor*>(
        lua_touserdata(L, 1));
    if (!acceptor || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_acceptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    acceptor->cancel(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

inline int tcp_acceptor_is_open(lua_State* L)
{
    auto a = reinterpret_cast<asio::ip::tcp::acceptor*>(lua_touserdata(L, 1));
    lua_pushboolean(L, a->is_open());
    return 1;
}

inline int tcp_acceptor_local_address(lua_State* L)
{
    auto acceptor = reinterpret_cast<asio::ip::tcp::acceptor*>(
        lua_touserdata(L, 1));
    boost::system::error_code ec;
    auto ep = acceptor->local_endpoint(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    auto addr = reinterpret_cast<asio::ip::address*>(
        lua_newuserdata(L, sizeof(asio::ip::address))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    setmetatable(L, -2);
    new (addr) asio::ip::address{ep.address()};
    return 1;
}

inline int tcp_acceptor_local_port(lua_State* L)
{
    auto a = reinterpret_cast<asio::ip::tcp::acceptor*>(lua_touserdata(L, 1));
    boost::system::error_code ec;
    auto ep = a->local_endpoint(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    lua_pushinteger(L, ep.port());
    return 1;
}

static int tcp_acceptor_mt_index(lua_State* L)
{
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("accept"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX, &tcp_acceptor_accept_key);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("listen"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, tcp_acceptor_listen);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("bind"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, tcp_acceptor_bind);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("open"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, tcp_acceptor_open);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("close"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, tcp_acceptor_close);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("set_option"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, tcp_acceptor_set_option);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("get_option"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, tcp_acceptor_get_option);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("cancel"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, tcp_acceptor_cancel);
                    return 1;
                }
            ),
            hana::make_pair(BOOST_HANA_STRING("is_open"), tcp_acceptor_is_open),
            hana::make_pair(
                BOOST_HANA_STRING("local_address"), tcp_acceptor_local_address),
            hana::make_pair(
                BOOST_HANA_STRING("local_port"), tcp_acceptor_local_port)
        ),
        [](std::string_view /*key*/, lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        },
        tostringview(L, 2),
        L
    );
}

static int tcp_get_address_info(lua_State* L)
{
    lua_settop(L, 3);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    // Lua BitOp underlying type is int32
    std::int32_t flags;
    switch (lua_type(L, 3)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    case LUA_TNIL:
        flags = asio::ip::resolver_base::address_configured |
            asio::ip::resolver_base::v4_mapped;
        break;
    case LUA_TNUMBER:
        flags = lua_tointeger(L, 3);
    }

    std::string host;
    switch (lua_type(L, 1)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    case LUA_TSTRING:
        host = tostringview(L, 1);
        break;
    case LUA_TUSERDATA: {
        if (!lua_getmetatable(L, 1)) {
            push(L, std::errc::invalid_argument, "arg", 1);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 1);
            return lua_error(L);
        }
        auto& a = *reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 1));
        host = a.to_string();
        flags |= asio::ip::resolver_base::numeric_host;
    }
    }

    switch (lua_type(L, 2)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    case LUA_TSTRING:
        break;
    case LUA_TNUMBER:
        flags |= asio::ip::resolver_base::numeric_service;
    }

    resolver_service* service = nullptr;

    for (auto& op: vm_ctx->pending_operations) {
        service = dynamic_cast<resolver_service*>(&op);
        if (service)
            break;
    }

    if (!service) {
        service = new resolver_service{vm_ctx->strand().context()};
        vm_ctx->pending_operations.push_back(*service);
    }

    lua_pushlightuserdata(L, service);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto service = reinterpret_cast<resolver_service*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            try {
                service->tcp_resolver.cancel();
            } catch (const boost::system::system_error&) {}
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    service->tcp_resolver.async_resolve(
        host,
        tostringview(L, 2),
        static_cast<asio::ip::tcp::resolver::flags>(flags),
        asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber](
                const boost::system::error_code& ec,
                asio::ip::tcp::resolver::results_type results
            ) {
                auto push_results = [&ec,&results](lua_State* fib) {
                    if (ec) {
                        lua_pushnil(fib);
                    } else {
                        lua_createtable(fib, /*narr=*/results.size(),
                                        /*nrec=*/0);
                        lua_pushliteral(fib, "address");
                        lua_pushliteral(fib, "port");
                        lua_pushliteral(fib, "host_name");
                        lua_pushliteral(fib, "service_name");

                        int i = 1;
                        for (const auto& res: results) {
                            lua_createtable(fib, /*narr=*/0, /*nrec=*/4);

                            lua_pushvalue(fib, -1 -4);
                            auto a = reinterpret_cast<asio::ip::address*>(
                                lua_newuserdata(fib, sizeof(asio::ip::address))
                            );
                            rawgetp(fib, LUA_REGISTRYINDEX, &ip_address_mt_key);
                            setmetatable(fib, -2);
                            new (a) asio::ip::address{res.endpoint().address()};
                            lua_rawset(fib, -3);

                            lua_pushvalue(fib, -1 -3);
                            lua_pushinteger(fib, res.endpoint().port());
                            lua_rawset(fib, -3);

                            lua_pushvalue(fib, -1 -2);
                            push(fib, res.host_name());
                            lua_rawset(fib, -3);

                            lua_pushvalue(fib, -1 -1);
                            push(fib, res.service_name());
                            lua_rawset(fib, -3);

                            lua_rawseti(fib, -6, i++);
                        }
                        lua_pop(fib, 4);
                    }
                };

                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(ec, push_results))
                    ));
            }
        )
    );

    return lua_yield(L, 0);
}

static int tcp_get_address_v4_info(lua_State* L)
{
    lua_settop(L, 3);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    // Lua BitOp underlying type is int32
    std::int32_t flags;
    switch (lua_type(L, 3)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    case LUA_TNIL:
        flags = 0;
        break;
    case LUA_TNUMBER:
        flags = lua_tointeger(L, 3);
    }

    std::string host;
    switch (lua_type(L, 1)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    case LUA_TSTRING:
        host = tostringview(L, 1);
        break;
    case LUA_TUSERDATA: {
        if (!lua_getmetatable(L, 1)) {
            push(L, std::errc::invalid_argument, "arg", 1);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 1);
            return lua_error(L);
        }
        auto& a = *reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 1));
        host = a.to_string();
        flags |= asio::ip::resolver_base::numeric_host;
    }
    }

    switch (lua_type(L, 2)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    case LUA_TSTRING:
        break;
    case LUA_TNUMBER:
        flags |= asio::ip::resolver_base::numeric_service;
    }

    resolver_service* service = nullptr;

    for (auto& op: vm_ctx->pending_operations) {
        service = dynamic_cast<resolver_service*>(&op);
        if (service)
            break;
    }

    if (!service) {
        service = new resolver_service{vm_ctx->strand().context()};
        vm_ctx->pending_operations.push_back(*service);
    }

    lua_pushlightuserdata(L, service);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto service = reinterpret_cast<resolver_service*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            try {
                service->tcp_resolver.cancel();
            } catch (const boost::system::system_error&) {}
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    service->tcp_resolver.async_resolve(
        asio::ip::tcp::v4(),
        host,
        tostringview(L, 2),
        static_cast<asio::ip::tcp::resolver::flags>(flags),
        asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber](
                const boost::system::error_code& ec,
                asio::ip::tcp::resolver::results_type results
            ) {
                auto push_results = [&ec,&results](lua_State* fib) {
                    if (ec) {
                        lua_pushnil(fib);
                    } else {
                        lua_createtable(fib, /*narr=*/results.size(),
                                        /*nrec=*/0);
                        lua_pushliteral(fib, "address");
                        lua_pushliteral(fib, "port");
                        lua_pushliteral(fib, "host_name");
                        lua_pushliteral(fib, "service_name");

                        int i = 1;
                        for (const auto& res: results) {
                            lua_createtable(fib, /*narr=*/0, /*nrec=*/4);

                            lua_pushvalue(fib, -1 -4);
                            auto a = reinterpret_cast<asio::ip::address*>(
                                lua_newuserdata(fib, sizeof(asio::ip::address))
                            );
                            rawgetp(fib, LUA_REGISTRYINDEX, &ip_address_mt_key);
                            setmetatable(fib, -2);
                            new (a) asio::ip::address{res.endpoint().address()};
                            lua_rawset(fib, -3);

                            lua_pushvalue(fib, -1 -3);
                            lua_pushinteger(fib, res.endpoint().port());
                            lua_rawset(fib, -3);

                            lua_pushvalue(fib, -1 -2);
                            push(fib, res.host_name());
                            lua_rawset(fib, -3);

                            lua_pushvalue(fib, -1 -1);
                            push(fib, res.service_name());
                            lua_rawset(fib, -3);

                            lua_rawseti(fib, -6, i++);
                        }
                        lua_pop(fib, 4);
                    }
                };

                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(ec, push_results))
                    ));
            }
        )
    );

    return lua_yield(L, 0);
}

static int tcp_get_address_v6_info(lua_State* L)
{
    lua_settop(L, 3);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    // Lua BitOp underlying type is int32
    std::int32_t flags;
    switch (lua_type(L, 3)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    case LUA_TNIL:
        flags = asio::ip::resolver_base::v4_mapped;
        break;
    case LUA_TNUMBER:
        flags = lua_tointeger(L, 3);
    }

    std::string host;
    switch (lua_type(L, 1)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    case LUA_TSTRING:
        host = tostringview(L, 1);
        break;
    case LUA_TUSERDATA: {
        if (!lua_getmetatable(L, 1)) {
            push(L, std::errc::invalid_argument, "arg", 1);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 1);
            return lua_error(L);
        }
        auto& a = *reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 1));
        host = a.to_string();
        flags |= asio::ip::resolver_base::numeric_host;
    }
    }

    switch (lua_type(L, 2)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    case LUA_TSTRING:
        break;
    case LUA_TNUMBER:
        flags |= asio::ip::resolver_base::numeric_service;
    }

    resolver_service* service = nullptr;

    for (auto& op: vm_ctx->pending_operations) {
        service = dynamic_cast<resolver_service*>(&op);
        if (service)
            break;
    }

    if (!service) {
        service = new resolver_service{vm_ctx->strand().context()};
        vm_ctx->pending_operations.push_back(*service);
    }

    lua_pushlightuserdata(L, service);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto service = reinterpret_cast<resolver_service*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            try {
                service->tcp_resolver.cancel();
            } catch (const boost::system::system_error&) {}
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    service->tcp_resolver.async_resolve(
        asio::ip::tcp::v6(),
        host,
        tostringview(L, 2),
        static_cast<asio::ip::tcp::resolver::flags>(flags),
        asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber](
                const boost::system::error_code& ec,
                asio::ip::tcp::resolver::results_type results
            ) {
                auto push_results = [&ec,&results](lua_State* fib) {
                    if (ec) {
                        lua_pushnil(fib);
                    } else {
                        lua_createtable(fib, /*narr=*/results.size(),
                                        /*nrec=*/0);
                        lua_pushliteral(fib, "address");
                        lua_pushliteral(fib, "port");
                        lua_pushliteral(fib, "host_name");
                        lua_pushliteral(fib, "service_name");

                        int i = 1;
                        for (const auto& res: results) {
                            lua_createtable(fib, /*narr=*/0, /*nrec=*/4);

                            lua_pushvalue(fib, -1 -4);
                            auto a = reinterpret_cast<asio::ip::address*>(
                                lua_newuserdata(fib, sizeof(asio::ip::address))
                            );
                            rawgetp(fib, LUA_REGISTRYINDEX, &ip_address_mt_key);
                            setmetatable(fib, -2);
                            new (a) asio::ip::address{res.endpoint().address()};
                            lua_rawset(fib, -3);

                            lua_pushvalue(fib, -1 -3);
                            lua_pushinteger(fib, res.endpoint().port());
                            lua_rawset(fib, -3);

                            lua_pushvalue(fib, -1 -2);
                            push(fib, res.host_name());
                            lua_rawset(fib, -3);

                            lua_pushvalue(fib, -1 -1);
                            push(fib, res.service_name());
                            lua_rawset(fib, -3);

                            lua_rawseti(fib, -6, i++);
                        }
                        lua_pop(fib, 4);
                    }
                };

                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(ec, push_results))
                    ));
            }
        )
    );

    return lua_yield(L, 0);
}

static int tcp_get_name_info(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TNUMBER);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto a = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 1));
    if (!a || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    resolver_service* service = nullptr;

    for (auto& op: vm_ctx->pending_operations) {
        service = dynamic_cast<resolver_service*>(&op);
        if (service)
            break;
    }

    if (!service) {
        service = new resolver_service{vm_ctx->strand().context()};
        vm_ctx->pending_operations.push_back(*service);
    }

    lua_pushlightuserdata(L, service);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto service = reinterpret_cast<resolver_service*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            try {
                service->tcp_resolver.cancel();
            } catch (const boost::system::system_error&) {}
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    service->tcp_resolver.async_resolve(
        asio::ip::tcp::endpoint(*a, lua_tointeger(L, 2)),
        asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber](
                const boost::system::error_code& ec,
                asio::ip::tcp::resolver::results_type results
            ) {
                auto push_results = [&ec,&results](lua_State* fib) {
                    if (ec) {
                        lua_pushnil(fib);
                    } else {
                        lua_createtable(fib, /*narr=*/results.size(),
                                        /*nrec=*/0);
                        lua_pushliteral(fib, "address");
                        lua_pushliteral(fib, "port");
                        lua_pushliteral(fib, "host_name");
                        lua_pushliteral(fib, "service_name");

                        int i = 1;
                        for (const auto& res: results) {
                            lua_createtable(fib, /*narr=*/0, /*nrec=*/4);

                            lua_pushvalue(fib, -1 -4);
                            auto a = reinterpret_cast<asio::ip::address*>(
                                lua_newuserdata(fib, sizeof(asio::ip::address))
                            );
                            rawgetp(fib, LUA_REGISTRYINDEX, &ip_address_mt_key);
                            setmetatable(fib, -2);
                            new (a) asio::ip::address{res.endpoint().address()};
                            lua_rawset(fib, -3);

                            lua_pushvalue(fib, -1 -3);
                            lua_pushinteger(fib, res.endpoint().port());
                            lua_rawset(fib, -3);

                            lua_pushvalue(fib, -1 -2);
                            push(fib, res.host_name());
                            lua_rawset(fib, -3);

                            lua_pushvalue(fib, -1 -1);
                            push(fib, res.service_name());
                            lua_rawset(fib, -3);

                            lua_rawseti(fib, -6, i++);
                        }
                        lua_pop(fib, 4);
                    }
                };

                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(ec, push_results))
                    ));
            }
        )
    );

    return lua_yield(L, 0);
}

static int udp_socket_new(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);
    auto a = reinterpret_cast<udp_socket*>(
        lua_newuserdata(L, sizeof(udp_socket))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &ip_udp_socket_mt_key);
    setmetatable(L, -2);
    new (a) tcp_socket{vm_ctx.strand().context()};
    return 1;
}

static int udp_socket_open(lua_State* L)
{
    lua_settop(L, 2);
    auto sock = reinterpret_cast<udp_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_udp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    switch (lua_type(L, 2)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    case LUA_TUSERDATA: {
        auto addr = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 2));
        if (!addr || !lua_getmetatable(L, 2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        boost::system::error_code ec;
        sock->socket.open(asio::ip::udp::endpoint{*addr, 0}.protocol(), ec);
        if (ec) {
            push(L, static_cast<std::error_code>(ec));
            return lua_error(L);
        }
        return 0;
    }
    case LUA_TSTRING:
        return dispatch_table::dispatch(
            hana::make_tuple(
                hana::make_pair(
                    BOOST_HANA_STRING("v4"),
                    [&]() -> int {
                        boost::system::error_code ec;
                        sock->socket.open(asio::ip::udp::v4(), ec);
                        if (ec) {
                            push(L, static_cast<std::error_code>(ec));
                            return lua_error(L);
                        }
                        return 0;
                    }
                ),
                hana::make_pair(
                    BOOST_HANA_STRING("v6"),
                    [&]() -> int {
                        boost::system::error_code ec;
                        sock->socket.open(asio::ip::udp::v6(), ec);
                        if (ec) {
                            push(L, static_cast<std::error_code>(ec));
                            return lua_error(L);
                        }
                        return 0;
                    }
                )
            ),
            [&](std::string_view /*key*/) -> int {
                push(L, std::errc::invalid_argument, "arg", 2);
                return lua_error(L);
            },
            tostringview(L, 2)
        );
    }
}

static int udp_socket_bind(lua_State* L)
{
    luaL_checktype(L, 3, LUA_TNUMBER);
    auto sock = reinterpret_cast<udp_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_udp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    switch (lua_type(L, 2)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    case LUA_TUSERDATA: {
        auto addr = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 2));
        if (!addr || !lua_getmetatable(L, 2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        asio::ip::udp::endpoint ep(*addr, lua_tointeger(L, 3));
        boost::system::error_code ec;
        sock->socket.bind(ep, ec);
        if (ec) {
            push(L, static_cast<std::error_code>(ec));
            return lua_error(L);
        }
        return 0;
    }
    case LUA_TSTRING: {
        boost::system::error_code ec;
        auto addr = asio::ip::make_address(lua_tostring(L, 2), ec);
        if (ec) {
            push(L, static_cast<std::error_code>(ec));
            return lua_error(L);
        }
        asio::ip::udp::endpoint ep(addr, lua_tointeger(L, 3));
        sock->socket.bind(ep, ec);
        if (ec) {
            push(L, static_cast<std::error_code>(ec));
            return lua_error(L);
        }
        return 0;
    }
    }
}

static int udp_socket_shutdown(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);

    auto sock = reinterpret_cast<udp_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_udp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    std::error_code ec;
    asio::ip::udp::socket::shutdown_type what;
    dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("receive"),
                [&]() { what = asio::ip::udp::socket::shutdown_receive; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("send"),
                [&]() { what = asio::ip::udp::socket::shutdown_send; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("both"),
                [&]() { what = asio::ip::udp::socket::shutdown_both; }
            )
        ),
        [&](std::string_view /*key*/) {
            ec = make_error_code(std::errc::invalid_argument);
        },
        tostringview(L, 2)
    );
    if (ec) {
        push(L, ec);
        return lua_error(L);
    }
    boost::system::error_code ec2;
    sock->socket.shutdown(what, ec2);
    if (ec2) {
        push(L, static_cast<std::error_code>(ec2));
        return lua_error(L);
    }
    return 0;
}

static int udp_socket_connect(lua_State* L)
{
    luaL_checktype(L, 3, LUA_TNUMBER);
    auto sock = reinterpret_cast<udp_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_udp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto addr = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 2));
    if (!addr || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    asio::ip::udp::endpoint ep(*addr, lua_tointeger(L, 3));
    boost::system::error_code ec;
    sock->socket.connect(ep, ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int udp_socket_close(lua_State* L)
{
    auto sock = reinterpret_cast<udp_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_udp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    sock->socket.close(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int udp_socket_cancel(lua_State* L)
{
    auto sock = reinterpret_cast<udp_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_udp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    sock->socket.cancel(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int udp_socket_set_option(lua_State* L)
{
    lua_settop(L, 3);
    luaL_checktype(L, 2, LUA_TSTRING);

    auto socket = reinterpret_cast<udp_socket*>(
        lua_touserdata(L, 1));
    if (!socket || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_udp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("debug"),
                [&]() -> int {
                    luaL_checktype(L, 3, LUA_TBOOLEAN);
                    asio::socket_base::debug o(lua_toboolean(L, 3));
                    socket->socket.set_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("broadcast"),
                [&]() -> int {
                    luaL_checktype(L, 3, LUA_TBOOLEAN);
                    asio::socket_base::broadcast o(lua_toboolean(L, 3));
                    socket->socket.set_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("do_not_route"),
                [&]() -> int {
                    luaL_checktype(L, 3, LUA_TBOOLEAN);
                    asio::socket_base::do_not_route o(lua_toboolean(L, 3));
                    socket->socket.set_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("send_buffer_size"),
                [&]() -> int {
                    luaL_checktype(L, 3, LUA_TNUMBER);
                    asio::socket_base::send_buffer_size o(lua_tointeger(L, 3));
                    socket->socket.set_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("receive_buffer_size"),
                [&]() -> int {
                    luaL_checktype(L, 3, LUA_TNUMBER);
                    asio::socket_base::receive_buffer_size o(
                        lua_tointeger(L, 3));
                    socket->socket.set_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("reuse_address"),
                [&]() -> int {
                    luaL_checktype(L, 3, LUA_TBOOLEAN);
                    asio::socket_base::reuse_address o(lua_toboolean(L, 3));
                    socket->socket.set_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("multicast_loop"),
                [&]() -> int {
                    luaL_checktype(L, 3, LUA_TBOOLEAN);
                    asio::ip::multicast::enable_loopback o(lua_toboolean(L, 3));
                    socket->socket.set_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("multicast_hops"),
                [&]() -> int {
                    luaL_checktype(L, 3, LUA_TNUMBER);
                    asio::ip::multicast::hops o(lua_tointeger(L, 3));
                    socket->socket.set_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("join_multicast_group"),
                [&]() -> int {
                    auto addr = reinterpret_cast<asio::ip::address*>(
                        lua_touserdata(L, 3));
                    if (!addr || !lua_getmetatable(L, 3)) {
                        push(L, std::errc::invalid_argument, "arg", 3);
                        return lua_error(L);
                    }
                    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
                    if (!lua_rawequal(L, -1, -2)) {
                        push(L, std::errc::invalid_argument, "arg", 3);
                        return lua_error(L);
                    }
                    asio::ip::multicast::join_group o(*addr);
                    socket->socket.set_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("leave_multicast_group"),
                [&]() -> int {
                    auto addr = reinterpret_cast<asio::ip::address*>(
                        lua_touserdata(L, 3));
                    if (!addr || !lua_getmetatable(L, 3)) {
                        push(L, std::errc::invalid_argument, "arg", 3);
                        return lua_error(L);
                    }
                    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
                    if (!lua_rawequal(L, -1, -2)) {
                        push(L, std::errc::invalid_argument, "arg", 3);
                        return lua_error(L);
                    }
                    asio::ip::multicast::leave_group o(*addr);
                    socket->socket.set_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("multicast_interface"),
                [&]() -> int {
                    auto addr = reinterpret_cast<asio::ip::address*>(
                        lua_touserdata(L, 3));
                    if (!addr || !lua_getmetatable(L, 3)) {
                        push(L, std::errc::invalid_argument, "arg", 3);
                        return lua_error(L);
                    }
                    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
                    if (!lua_rawequal(L, -1, -2)) {
                        push(L, std::errc::invalid_argument, "arg", 3);
                        return lua_error(L);
                    }
                    if (!addr->is_v4()) {
                        push(L, std::errc::invalid_argument, "arg", 3);
                        return lua_error(L);
                    }
                    asio::ip::multicast::outbound_interface o(addr->to_v4());
                    socket->socket.set_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("unicast_hops"),
                [&]() -> int {
                    luaL_checktype(L, 3, LUA_TNUMBER);
                    asio::ip::unicast::hops o(lua_tointeger(L, 3));
                    socket->socket.set_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("v6_only"),
                [&]() -> int {
                    luaL_checktype(L, 3, LUA_TBOOLEAN);
                    asio::ip::v6_only o(lua_toboolean(L, 3));
                    socket->socket.set_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            )
        ),
        [L](std::string_view /*key*/) -> int {
            push(L, std::errc::not_supported);
            return lua_error(L);
        },
        tostringview(L, 2)
    );
    return 0;
}

static int udp_socket_get_option(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);

    auto socket = reinterpret_cast<udp_socket*>(
        lua_touserdata(L, 1));
    if (!socket || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_udp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("debug"),
                [&]() -> int {
                    asio::socket_base::debug o;
                    socket->socket.get_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    lua_pushboolean(L, o.value());
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("broadcast"),
                [&]() -> int {
                    asio::socket_base::broadcast o;
                    socket->socket.get_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    lua_pushboolean(L, o.value());
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("do_not_route"),
                [&]() -> int {
                    asio::socket_base::do_not_route o;
                    socket->socket.get_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    lua_pushboolean(L, o.value());
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("send_buffer_size"),
                [&]() -> int {
                    asio::socket_base::send_buffer_size o;
                    socket->socket.get_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    lua_pushinteger(L, o.value());
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("receive_buffer_size"),
                [&]() -> int {
                    asio::socket_base::receive_buffer_size o;
                    socket->socket.get_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    lua_pushinteger(L, o.value());
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("reuse_address"),
                [&]() -> int {
                    asio::socket_base::reuse_address o;
                    socket->socket.get_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    lua_pushboolean(L, o.value());
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("multicast_loop"),
                [&]() -> int {
                    asio::ip::multicast::enable_loopback o;
                    socket->socket.get_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    lua_pushboolean(L, o.value());
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("multicast_hops"),
                [&]() -> int {
                    asio::ip::multicast::hops o;
                    socket->socket.get_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    lua_pushinteger(L, o.value());
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("unicast_hops"),
                [&]() -> int {
                    asio::ip::unicast::hops o;
                    socket->socket.get_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    lua_pushinteger(L, o.value());
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("v6_only"),
                [&]() -> int {
                    asio::ip::v6_only o;
                    socket->socket.get_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    lua_pushboolean(L, o.value());
                    return 1;
                }
            )
        ),
        [L](std::string_view /*key*/) -> int {
            push(L, std::errc::not_supported);
            return lua_error(L);
        },
        tostringview(L, 2)
    );
}

static int udp_socket_receive(lua_State* L)
{
    lua_settop(L, 3);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto sock = reinterpret_cast<udp_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_udp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto bs = reinterpret_cast<byte_span_handle*>(lua_touserdata(L, 2));
    if (!bs || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &byte_span_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    // Lua BitOp underlying type is int32
    std::int32_t flags;
    switch (lua_type(L, 3)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    case LUA_TNIL:
        flags = 0;
        break;
    case LUA_TNUMBER:
        flags = lua_tointeger(L, 3);
    }

    lua_pushvalue(L, 1);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto s = reinterpret_cast<udp_socket*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            boost::system::error_code ignored_ec;
            s->socket.cancel(ignored_ec);
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    auto remote_sender = std::make_shared<asio::ip::udp::endpoint>();

    ++sock->nbusy;
    sock->socket.async_receive_from(
        asio::buffer(bs->data.get(), bs->size),
        *remote_sender,
        flags,
        asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,remote_sender,buf=bs->data,sock](
                const boost::system::error_code& ec,
                std::size_t bytes_transferred
            ) {
                boost::ignore_unused(buf);
                if (!vm_ctx->valid())
                    return;

                --sock->nbusy;

                auto addr_pusher = [&remote_sender](lua_State* L) {
                    auto a = reinterpret_cast<asio::ip::address*>(
                        lua_newuserdata(L, sizeof(asio::ip::address))
                    );
                    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
                    setmetatable(L, -2);
                    new (a) asio::ip::address{remote_sender->address()};
                    return 1;
                };
                auto port = remote_sender->port();

                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(
                                ec, bytes_transferred, addr_pusher, port))));
            }
        )
    );

    return lua_yield(L, 0);
}

static int udp_socket_send(lua_State* L)
{
    lua_settop(L, 3);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto sock = reinterpret_cast<udp_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_udp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto bs = reinterpret_cast<byte_span_handle*>(lua_touserdata(L, 2));
    if (!bs || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &byte_span_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    // Lua BitOp underlying type is int32
    std::int32_t flags;
    switch (lua_type(L, 3)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    case LUA_TNIL:
        flags = 0;
        break;
    case LUA_TNUMBER:
        flags = lua_tointeger(L, 3);
    }

    lua_pushvalue(L, 1);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto s = reinterpret_cast<udp_socket*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            boost::system::error_code ignored_ec;
            s->socket.cancel(ignored_ec);
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    ++sock->nbusy;
    sock->socket.async_send(
        asio::buffer(bs->data.get(), bs->size),
        flags,
        asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,buf=bs->data,sock](
                const boost::system::error_code& ec,
                std::size_t bytes_transferred
            ) {
                boost::ignore_unused(buf);
                if (!vm_ctx->valid())
                    return;

                --sock->nbusy;

                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(ec, bytes_transferred))));
            }
        )
    );

    return lua_yield(L, 0);
}

static int udp_socket_send_to(lua_State* L)
{
    lua_settop(L, 5);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto sock = reinterpret_cast<udp_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_udp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto bs = reinterpret_cast<byte_span_handle*>(lua_touserdata(L, 2));
    if (!bs || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &byte_span_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    auto addr = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 3));
    if (!addr || !lua_getmetatable(L, 3)) {
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    }

    std::uint16_t port;
    if (lua_type(L, 4) != LUA_TNUMBER) {
        push(L, std::errc::invalid_argument, "arg", 4);
        return lua_error(L);
    }
    port = lua_tointeger(L, 4);

    // Lua BitOp underlying type is int32
    std::int32_t flags;
    switch (lua_type(L, 5)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 5);
        return lua_error(L);
    case LUA_TNIL:
        flags = 0;
        break;
    case LUA_TNUMBER:
        flags = lua_tointeger(L, 5);
    }

    lua_pushvalue(L, 1);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto s = reinterpret_cast<udp_socket*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            boost::system::error_code ignored_ec;
            s->socket.cancel(ignored_ec);
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    ++sock->nbusy;
    sock->socket.async_send_to(
        asio::buffer(bs->data.get(), bs->size),
        asio::ip::udp::endpoint{*addr, port},
        flags,
        asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,buf=bs->data,sock](
                const boost::system::error_code& ec,
                std::size_t bytes_transferred
            ) {
                boost::ignore_unused(buf);
                if (!vm_ctx->valid())
                    return;

                --sock->nbusy;

                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(ec, bytes_transferred))));
            }
        )
    );

    return lua_yield(L, 0);
}

static int udp_socket_io_control(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);

    auto socket = reinterpret_cast<udp_socket*>(
        lua_touserdata(L, 1));
    if (!socket || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_udp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("bytes_readable"),
                [&]() -> int {
                    asio::socket_base::bytes_readable command;
                    socket->socket.io_control(command, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    lua_pushnumber(L, command.get());
                    return 1;
                }
            )
        ),
        [L](std::string_view /*key*/) -> int {
            push(L, std::errc::not_supported);
            return lua_error(L);
        },
        tostringview(L, 2)
    );
    return 0;
}

inline int udp_socket_is_open(lua_State* L)
{
    auto sock = reinterpret_cast<udp_socket*>(lua_touserdata(L, 1));
    lua_pushboolean(L, sock->socket.is_open());
    return 1;
}

inline int udp_socket_local_address(lua_State* L)
{
    auto sock = reinterpret_cast<udp_socket*>(lua_touserdata(L, 1));
    boost::system::error_code ec;
    auto ep = sock->socket.local_endpoint(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    auto addr = reinterpret_cast<asio::ip::address*>(
        lua_newuserdata(L, sizeof(asio::ip::address))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    setmetatable(L, -2);
    new (addr) asio::ip::address{ep.address()};
    return 1;
}

inline int udp_socket_local_port(lua_State* L)
{
    auto sock = reinterpret_cast<udp_socket*>(lua_touserdata(L, 1));
    boost::system::error_code ec;
    auto ep = sock->socket.local_endpoint(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    lua_pushinteger(L, ep.port());
    return 1;
}

inline int udp_socket_remote_address(lua_State* L)
{
    auto sock = reinterpret_cast<udp_socket*>(lua_touserdata(L, 1));
    boost::system::error_code ec;
    auto ep = sock->socket.remote_endpoint(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    auto addr = reinterpret_cast<asio::ip::address*>(
        lua_newuserdata(L, sizeof(asio::ip::address))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    setmetatable(L, -2);
    new (addr) asio::ip::address{ep.address()};
    return 1;
}

inline int udp_socket_remote_port(lua_State* L)
{
    auto sock = reinterpret_cast<udp_socket*>(lua_touserdata(L, 1));
    boost::system::error_code ec;
    auto ep = sock->socket.remote_endpoint(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    lua_pushinteger(L, ep.port());
    return 1;
}

static int udp_socket_mt_index(lua_State* L)
{
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("open"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, udp_socket_open);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("bind"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, udp_socket_bind);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("shutdown"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, udp_socket_shutdown);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("connect"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, udp_socket_connect);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("close"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, udp_socket_close);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("cancel"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, udp_socket_cancel);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("set_option"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, udp_socket_set_option);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("get_option"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, udp_socket_get_option);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("receive"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX, &udp_socket_receive_key);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("send"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX, &udp_socket_send_key);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("send_to"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX, &udp_socket_send_to_key);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("io_control"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, udp_socket_io_control);
                    return 1;
                }
            ),
            hana::make_pair(BOOST_HANA_STRING("is_open"), udp_socket_is_open),
            hana::make_pair(
                BOOST_HANA_STRING("local_address"), udp_socket_local_address),
            hana::make_pair(
                BOOST_HANA_STRING("local_port"), udp_socket_local_port),
            hana::make_pair(
                BOOST_HANA_STRING("remote_address"), udp_socket_remote_address),
            hana::make_pair(
                BOOST_HANA_STRING("remote_port"), udp_socket_remote_port)
        ),
        [](std::string_view /*key*/, lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        },
        tostringview(L, 2),
        L
    );
}

static int udp_get_address_info(lua_State* L)
{
    lua_settop(L, 3);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    // Lua BitOp underlying type is int32
    std::int32_t flags;
    switch (lua_type(L, 3)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    case LUA_TNIL:
        flags = asio::ip::resolver_base::address_configured |
            asio::ip::resolver_base::v4_mapped;
        break;
    case LUA_TNUMBER:
        flags = lua_tointeger(L, 3);
    }

    std::string host;
    switch (lua_type(L, 1)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    case LUA_TSTRING:
        host = tostringview(L, 1);
        break;
    case LUA_TUSERDATA: {
        if (!lua_getmetatable(L, 1)) {
            push(L, std::errc::invalid_argument, "arg", 1);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 1);
            return lua_error(L);
        }
        auto& a = *reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 1));
        host = a.to_string();
        flags |= asio::ip::resolver_base::numeric_host;
    }
    }

    switch (lua_type(L, 2)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    case LUA_TSTRING:
        break;
    case LUA_TNUMBER:
        flags |= asio::ip::resolver_base::numeric_service;
    }

    resolver_service* service = nullptr;

    for (auto& op: vm_ctx->pending_operations) {
        service = dynamic_cast<resolver_service*>(&op);
        if (service)
            break;
    }

    if (!service) {
        service = new resolver_service{vm_ctx->strand().context()};
        vm_ctx->pending_operations.push_back(*service);
    }

    lua_pushlightuserdata(L, service);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto service = reinterpret_cast<resolver_service*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            try {
                service->udp_resolver.cancel();
            } catch (const boost::system::system_error&) {}
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    service->udp_resolver.async_resolve(
        host,
        tostringview(L, 2),
        static_cast<asio::ip::udp::resolver::flags>(flags),
        asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber](
                const boost::system::error_code& ec,
                asio::ip::udp::resolver::results_type results
            ) {
                auto push_results = [&ec,&results](lua_State* fib) {
                    if (ec) {
                        lua_pushnil(fib);
                    } else {
                        lua_createtable(fib, /*narr=*/results.size(),
                                        /*nrec=*/0);
                        lua_pushliteral(fib, "address");
                        lua_pushliteral(fib, "port");
                        lua_pushliteral(fib, "host_name");
                        lua_pushliteral(fib, "service_name");

                        int i = 1;
                        for (const auto& res: results) {
                            lua_createtable(fib, /*narr=*/0, /*nrec=*/4);

                            lua_pushvalue(fib, -1 -4);
                            auto a = reinterpret_cast<asio::ip::address*>(
                                lua_newuserdata(fib, sizeof(asio::ip::address))
                            );
                            rawgetp(fib, LUA_REGISTRYINDEX, &ip_address_mt_key);
                            setmetatable(fib, -2);
                            new (a) asio::ip::address{res.endpoint().address()};
                            lua_rawset(fib, -3);

                            lua_pushvalue(fib, -1 -3);
                            lua_pushinteger(fib, res.endpoint().port());
                            lua_rawset(fib, -3);

                            lua_pushvalue(fib, -1 -2);
                            push(fib, res.host_name());
                            lua_rawset(fib, -3);

                            lua_pushvalue(fib, -1 -1);
                            push(fib, res.service_name());
                            lua_rawset(fib, -3);

                            lua_rawseti(fib, -6, i++);
                        }
                        lua_pop(fib, 4);
                    }
                };

                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(ec, push_results))
                    ));
            }
        )
    );

    return lua_yield(L, 0);
}

static int udp_get_address_v4_info(lua_State* L)
{
    lua_settop(L, 3);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    // Lua BitOp underlying type is int32
    std::int32_t flags;
    switch (lua_type(L, 3)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    case LUA_TNIL:
        flags = 0;
        break;
    case LUA_TNUMBER:
        flags = lua_tointeger(L, 3);
    }

    std::string host;
    switch (lua_type(L, 1)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    case LUA_TSTRING:
        host = tostringview(L, 1);
        break;
    case LUA_TUSERDATA: {
        if (!lua_getmetatable(L, 1)) {
            push(L, std::errc::invalid_argument, "arg", 1);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 1);
            return lua_error(L);
        }
        auto& a = *reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 1));
        host = a.to_string();
        flags |= asio::ip::resolver_base::numeric_host;
    }
    }

    switch (lua_type(L, 2)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    case LUA_TSTRING:
        break;
    case LUA_TNUMBER:
        flags |= asio::ip::resolver_base::numeric_service;
    }

    resolver_service* service = nullptr;

    for (auto& op: vm_ctx->pending_operations) {
        service = dynamic_cast<resolver_service*>(&op);
        if (service)
            break;
    }

    if (!service) {
        service = new resolver_service{vm_ctx->strand().context()};
        vm_ctx->pending_operations.push_back(*service);
    }

    lua_pushlightuserdata(L, service);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto service = reinterpret_cast<resolver_service*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            try {
                service->udp_resolver.cancel();
            } catch (const boost::system::system_error&) {}
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    service->udp_resolver.async_resolve(
        asio::ip::udp::v4(),
        host,
        tostringview(L, 2),
        static_cast<asio::ip::udp::resolver::flags>(flags),
        asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber](
                const boost::system::error_code& ec,
                asio::ip::udp::resolver::results_type results
            ) {
                auto push_results = [&ec,&results](lua_State* fib) {
                    if (ec) {
                        lua_pushnil(fib);
                    } else {
                        lua_createtable(fib, /*narr=*/results.size(),
                                        /*nrec=*/0);
                        lua_pushliteral(fib, "address");
                        lua_pushliteral(fib, "port");
                        lua_pushliteral(fib, "host_name");
                        lua_pushliteral(fib, "service_name");

                        int i = 1;
                        for (const auto& res: results) {
                            lua_createtable(fib, /*narr=*/0, /*nrec=*/4);

                            lua_pushvalue(fib, -1 -4);
                            auto a = reinterpret_cast<asio::ip::address*>(
                                lua_newuserdata(fib, sizeof(asio::ip::address))
                            );
                            rawgetp(fib, LUA_REGISTRYINDEX, &ip_address_mt_key);
                            setmetatable(fib, -2);
                            new (a) asio::ip::address{res.endpoint().address()};
                            lua_rawset(fib, -3);

                            lua_pushvalue(fib, -1 -3);
                            lua_pushinteger(fib, res.endpoint().port());
                            lua_rawset(fib, -3);

                            lua_pushvalue(fib, -1 -2);
                            push(fib, res.host_name());
                            lua_rawset(fib, -3);

                            lua_pushvalue(fib, -1 -1);
                            push(fib, res.service_name());
                            lua_rawset(fib, -3);

                            lua_rawseti(fib, -6, i++);
                        }
                        lua_pop(fib, 4);
                    }
                };

                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(ec, push_results))
                    ));
            }
        )
    );

    return lua_yield(L, 0);
}

static int udp_get_address_v6_info(lua_State* L)
{
    lua_settop(L, 3);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    // Lua BitOp underlying type is int32
    std::int32_t flags;
    switch (lua_type(L, 3)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    case LUA_TNIL:
        flags = asio::ip::resolver_base::v4_mapped;
        break;
    case LUA_TNUMBER:
        flags = lua_tointeger(L, 3);
    }

    std::string host;
    switch (lua_type(L, 1)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    case LUA_TSTRING:
        host = tostringview(L, 1);
        break;
    case LUA_TUSERDATA: {
        if (!lua_getmetatable(L, 1)) {
            push(L, std::errc::invalid_argument, "arg", 1);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 1);
            return lua_error(L);
        }
        auto& a = *reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 1));
        host = a.to_string();
        flags |= asio::ip::resolver_base::numeric_host;
    }
    }

    switch (lua_type(L, 2)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    case LUA_TSTRING:
        break;
    case LUA_TNUMBER:
        flags |= asio::ip::resolver_base::numeric_service;
    }

    resolver_service* service = nullptr;

    for (auto& op: vm_ctx->pending_operations) {
        service = dynamic_cast<resolver_service*>(&op);
        if (service)
            break;
    }

    if (!service) {
        service = new resolver_service{vm_ctx->strand().context()};
        vm_ctx->pending_operations.push_back(*service);
    }

    lua_pushlightuserdata(L, service);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto service = reinterpret_cast<resolver_service*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            try {
                service->udp_resolver.cancel();
            } catch (const boost::system::system_error&) {}
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    service->udp_resolver.async_resolve(
        asio::ip::udp::v6(),
        host,
        tostringview(L, 2),
        static_cast<asio::ip::udp::resolver::flags>(flags),
        asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber](
                const boost::system::error_code& ec,
                asio::ip::udp::resolver::results_type results
            ) {
                auto push_results = [&ec,&results](lua_State* fib) {
                    if (ec) {
                        lua_pushnil(fib);
                    } else {
                        lua_createtable(fib, /*narr=*/results.size(),
                                        /*nrec=*/0);
                        lua_pushliteral(fib, "address");
                        lua_pushliteral(fib, "port");
                        lua_pushliteral(fib, "host_name");
                        lua_pushliteral(fib, "service_name");

                        int i = 1;
                        for (const auto& res: results) {
                            lua_createtable(fib, /*narr=*/0, /*nrec=*/4);

                            lua_pushvalue(fib, -1 -4);
                            auto a = reinterpret_cast<asio::ip::address*>(
                                lua_newuserdata(fib, sizeof(asio::ip::address))
                            );
                            rawgetp(fib, LUA_REGISTRYINDEX, &ip_address_mt_key);
                            setmetatable(fib, -2);
                            new (a) asio::ip::address{res.endpoint().address()};
                            lua_rawset(fib, -3);

                            lua_pushvalue(fib, -1 -3);
                            lua_pushinteger(fib, res.endpoint().port());
                            lua_rawset(fib, -3);

                            lua_pushvalue(fib, -1 -2);
                            push(fib, res.host_name());
                            lua_rawset(fib, -3);

                            lua_pushvalue(fib, -1 -1);
                            push(fib, res.service_name());
                            lua_rawset(fib, -3);

                            lua_rawseti(fib, -6, i++);
                        }
                        lua_pop(fib, 4);
                    }
                };

                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(ec, push_results))
                    ));
            }
        )
    );

    return lua_yield(L, 0);
}

static int udp_get_name_info(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TNUMBER);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto a = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 1));
    if (!a || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    resolver_service* service = nullptr;

    for (auto& op: vm_ctx->pending_operations) {
        service = dynamic_cast<resolver_service*>(&op);
        if (service)
            break;
    }

    if (!service) {
        service = new resolver_service{vm_ctx->strand().context()};
        vm_ctx->pending_operations.push_back(*service);
    }

    lua_pushlightuserdata(L, service);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto service = reinterpret_cast<resolver_service*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            try {
                service->udp_resolver.cancel();
            } catch (const boost::system::system_error&) {}
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    service->udp_resolver.async_resolve(
        asio::ip::udp::endpoint(*a, lua_tointeger(L, 2)),
        asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber](
                const boost::system::error_code& ec,
                asio::ip::udp::resolver::results_type results
            ) {
                auto push_results = [&ec,&results](lua_State* fib) {
                    if (ec) {
                        lua_pushnil(fib);
                    } else {
                        lua_createtable(fib, /*narr=*/results.size(),
                                        /*nrec=*/0);
                        lua_pushliteral(fib, "address");
                        lua_pushliteral(fib, "port");
                        lua_pushliteral(fib, "host_name");
                        lua_pushliteral(fib, "service_name");

                        int i = 1;
                        for (const auto& res: results) {
                            lua_createtable(fib, /*narr=*/0, /*nrec=*/4);

                            lua_pushvalue(fib, -1 -4);
                            auto a = reinterpret_cast<asio::ip::address*>(
                                lua_newuserdata(fib, sizeof(asio::ip::address))
                            );
                            rawgetp(fib, LUA_REGISTRYINDEX, &ip_address_mt_key);
                            setmetatable(fib, -2);
                            new (a) asio::ip::address{res.endpoint().address()};
                            lua_rawset(fib, -3);

                            lua_pushvalue(fib, -1 -3);
                            lua_pushinteger(fib, res.endpoint().port());
                            lua_rawset(fib, -3);

                            lua_pushvalue(fib, -1 -2);
                            push(fib, res.host_name());
                            lua_rawset(fib, -3);

                            lua_pushvalue(fib, -1 -1);
                            push(fib, res.service_name());
                            lua_rawset(fib, -3);

                            lua_rawseti(fib, -6, i++);
                        }
                        lua_pop(fib, 4);
                    }
                };

                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(ec, push_results))
                    ));
            }
        )
    );

    return lua_yield(L, 0);
}

void init_ip(lua_State* L)
{
    lua_pushlightuserdata(L, &ip_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/6);

        lua_pushliteral(L, "host_name");
        lua_pushcfunction(L, ip_host_name);
        lua_rawset(L, -3);

        lua_pushliteral(L, "address");
        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/6);

            lua_pushliteral(L, "new");
            lua_pushcfunction(L, address_new);
            lua_rawset(L, -3);

            lua_pushliteral(L, "any_v4");
            lua_pushcfunction(L, address_any_v4);
            lua_rawset(L, -3);

            lua_pushliteral(L, "any_v6");
            lua_pushcfunction(L, address_any_v6);
            lua_rawset(L, -3);

            lua_pushliteral(L, "loopback_v4");
            lua_pushcfunction(L, address_loopback_v4);
            lua_rawset(L, -3);

            lua_pushliteral(L, "loopback_v6");
            lua_pushcfunction(L, address_loopback_v6);
            lua_rawset(L, -3);

            lua_pushliteral(L, "broadcast_v4");
            lua_pushcfunction(L, address_broadcast_v4);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);

        lua_pushliteral(L, "message_flag");
        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/4);

            lua_pushliteral(L, "do_not_route");
            lua_pushinteger(L, asio::socket_base::message_do_not_route);
            lua_rawset(L, -3);

            lua_pushliteral(L, "end_of_record");
            lua_pushinteger(L, asio::socket_base::message_end_of_record);
            lua_rawset(L, -3);

            lua_pushliteral(L, "out_of_band");
            lua_pushinteger(L, asio::socket_base::message_out_of_band);
            lua_rawset(L, -3);

            lua_pushliteral(L, "peek");
            lua_pushinteger(L, asio::socket_base::message_peek);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);

        lua_pushliteral(L, "address_info_flag");
        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/7);

            lua_pushliteral(L, "address_configured");
            lua_pushinteger(L, asio::ip::resolver_base::address_configured);
            lua_rawset(L, -3);

            lua_pushliteral(L, "all_matching");
            lua_pushinteger(L, asio::ip::resolver_base::all_matching);
            lua_rawset(L, -3);

            lua_pushliteral(L, "canonical_name");
            lua_pushinteger(L, asio::ip::resolver_base::canonical_name);
            lua_rawset(L, -3);

            lua_pushliteral(L, "passive");
            lua_pushinteger(L, asio::ip::resolver_base::passive);
            lua_rawset(L, -3);

            lua_pushliteral(L, "v4_mapped");
            lua_pushinteger(L, asio::ip::resolver_base::v4_mapped);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);

        lua_pushliteral(L, "tcp");
        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/6);

            lua_pushliteral(L, "socket");
            {
                lua_createtable(L, /*narr=*/0, /*nrec=*/1);

                lua_pushliteral(L, "new");
                lua_pushcfunction(L, tcp_socket_new);
                lua_rawset(L, -3);
            }
            lua_rawset(L, -3);

            lua_pushliteral(L, "acceptor");
            {
                lua_createtable(L, /*narr=*/0, /*nrec=*/1);

                lua_pushliteral(L, "new");
                lua_pushcfunction(L, tcp_acceptor_new);
                lua_rawset(L, -3);
            }
            lua_rawset(L, -3);

            lua_pushliteral(L, "get_address_info");
            {
                int res = luaL_loadbuffer(
                    L, reinterpret_cast<char*>(resolve_bytecode),
                    resolve_bytecode_size, nullptr);
                assert(res == 0); boost::ignore_unused(res);
                rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
                lua_pushcfunction(L, tcp_get_address_info);
                lua_call(L, 2, 1);
            }
            lua_rawset(L, -3);

            lua_pushliteral(L, "get_address_v4_info");
            {
                int res = luaL_loadbuffer(
                    L, reinterpret_cast<char*>(resolve_bytecode),
                    resolve_bytecode_size, nullptr);
                assert(res == 0); boost::ignore_unused(res);
                rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
                lua_pushcfunction(L, tcp_get_address_v4_info);
                lua_call(L, 2, 1);
            }
            lua_rawset(L, -3);

            lua_pushliteral(L, "get_address_v6_info");
            {
                int res = luaL_loadbuffer(
                    L, reinterpret_cast<char*>(resolve_bytecode),
                    resolve_bytecode_size, nullptr);
                assert(res == 0); boost::ignore_unused(res);
                rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
                lua_pushcfunction(L, tcp_get_address_v6_info);
                lua_call(L, 2, 1);
            }
            lua_rawset(L, -3);

            lua_pushliteral(L, "get_name_info");
            {
                int res = luaL_loadbuffer(
                    L, reinterpret_cast<char*>(resolve_bytecode),
                    resolve_bytecode_size, nullptr);
                assert(res == 0); boost::ignore_unused(res);
                rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
                lua_pushcfunction(L, tcp_get_name_info);
                lua_call(L, 2, 1);
            }
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);

        lua_pushliteral(L, "udp");
        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/5);

            lua_pushliteral(L, "socket");
            {
                lua_createtable(L, /*narr=*/0, /*nrec=*/1);

                lua_pushliteral(L, "new");
                lua_pushcfunction(L, udp_socket_new);
                lua_rawset(L, -3);
            }
            lua_rawset(L, -3);

            lua_pushliteral(L, "get_address_info");
            {
                int res = luaL_loadbuffer(
                    L, reinterpret_cast<char*>(resolve_bytecode),
                    resolve_bytecode_size, nullptr);
                assert(res == 0); boost::ignore_unused(res);
                rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
                lua_pushcfunction(L, udp_get_address_info);
                lua_call(L, 2, 1);
            }
            lua_rawset(L, -3);

            lua_pushliteral(L, "get_address_v4_info");
            {
                int res = luaL_loadbuffer(
                    L, reinterpret_cast<char*>(resolve_bytecode),
                    resolve_bytecode_size, nullptr);
                assert(res == 0); boost::ignore_unused(res);
                rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
                lua_pushcfunction(L, udp_get_address_v4_info);
                lua_call(L, 2, 1);
            }
            lua_rawset(L, -3);

            lua_pushliteral(L, "get_address_v6_info");
            {
                int res = luaL_loadbuffer(
                    L, reinterpret_cast<char*>(resolve_bytecode),
                    resolve_bytecode_size, nullptr);
                assert(res == 0); boost::ignore_unused(res);
                rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
                lua_pushcfunction(L, udp_get_address_v6_info);
                lua_call(L, 2, 1);
            }
            lua_rawset(L, -3);

            lua_pushliteral(L, "get_name_info");
            {
                int res = luaL_loadbuffer(
                    L, reinterpret_cast<char*>(resolve_bytecode),
                    resolve_bytecode_size, nullptr);
                assert(res == 0); boost::ignore_unused(res);
                rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
                lua_pushcfunction(L, udp_get_name_info);
                lua_call(L, 2, 1);
            }
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &ip_address_mt_key);
    {
        static_assert(std::is_trivially_destructible_v<asio::ip::address>);

        lua_createtable(L, /*narr=*/0, /*nrec=*/7);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "ip.address");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, address_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__newindex");
        lua_pushcfunction(L, address_mt_newindex);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__tostring");
        lua_pushcfunction(L, address_mt_tostring);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__eq");
        lua_pushcfunction(L, address_mt_eq);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__lt");
        lua_pushcfunction(L, address_mt_lt);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__le");
        lua_pushcfunction(L, address_mt_le);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &ip_tcp_socket_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "ip.tcp.socket");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, tcp_socket_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<tcp_socket>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &ip_tcp_acceptor_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "ip.tcp.acceptor");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, tcp_acceptor_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<asio::ip::tcp::acceptor>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &ip_udp_socket_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "ip.udp.socket");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, udp_socket_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<udp_socket>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &tcp_socket_connect_key);
    int res = luaL_loadbuffer(L, reinterpret_cast<char*>(connect_bytecode),
                              connect_bytecode_size, nullptr);
    assert(res == 0); boost::ignore_unused(res);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, tcp_socket_connect);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &tcp_socket_read_some_key);
    res = luaL_loadbuffer(L, reinterpret_cast<char*>(data_op_bytecode),
                          data_op_bytecode_size, nullptr);
    assert(res == 0); boost::ignore_unused(res);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, tcp_socket_read_some);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &tcp_socket_write_some_key);
    res = luaL_loadbuffer(L, reinterpret_cast<char*>(data_op_bytecode),
                          data_op_bytecode_size, nullptr);
    assert(res == 0); boost::ignore_unused(res);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, tcp_socket_write_some);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &tcp_socket_receive_key);
    res = luaL_loadbuffer(L, reinterpret_cast<char*>(data_op_bytecode),
                          data_op_bytecode_size, nullptr);
    assert(res == 0); boost::ignore_unused(res);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, tcp_socket_receive);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &tcp_socket_send_key);
    res = luaL_loadbuffer(L, reinterpret_cast<char*>(data_op_bytecode),
                          data_op_bytecode_size, nullptr);
    assert(res == 0); boost::ignore_unused(res);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, tcp_socket_send);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

#if BOOST_OS_WINDOWS && EMILUA_CONFIG_ENABLE_FILE_IO
    lua_pushlightuserdata(L, &tcp_socket_send_file_key);
    res = luaL_loadbuffer(L, reinterpret_cast<char*>(data_op_bytecode),
                          data_op_bytecode_size, nullptr);
    assert(res == 0); boost::ignore_unused(res);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, tcp_socket_send_file);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);
#endif // BOOST_OS_WINDOWS && EMILUA_CONFIG_ENABLE_FILE_IO

    lua_pushlightuserdata(L, &tcp_socket_wait_key);
    res = luaL_loadbuffer(L, reinterpret_cast<char*>(data_op_bytecode),
                          data_op_bytecode_size, nullptr);
    assert(res == 0); boost::ignore_unused(res);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, tcp_socket_wait);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &tcp_acceptor_accept_key);
    res = luaL_loadbuffer(L, reinterpret_cast<char*>(accept_bytecode),
                          accept_bytecode_size, nullptr);
    assert(res == 0); boost::ignore_unused(res);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, tcp_acceptor_accept);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &udp_socket_receive_key);
    res = luaL_loadbuffer(L, reinterpret_cast<char*>(data_op_bytecode),
                          data_op_bytecode_size, nullptr);
    assert(res == 0); boost::ignore_unused(res);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, udp_socket_receive);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &udp_socket_send_key);
    res = luaL_loadbuffer(L, reinterpret_cast<char*>(data_op_bytecode),
                          data_op_bytecode_size, nullptr);
    assert(res == 0); boost::ignore_unused(res);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, udp_socket_send);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &udp_socket_send_to_key);
    res = luaL_loadbuffer(L, reinterpret_cast<char*>(data_op_bytecode),
                          data_op_bytecode_size, nullptr);
    assert(res == 0); boost::ignore_unused(res);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, udp_socket_send_to);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);
}

} // namespace emilua
