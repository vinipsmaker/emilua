/* Copyright (c) 2022 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#include <boost/asio/local/connect_pair.hpp>
#include <boost/asio/write.hpp>

#include <emilua/dispatch_table.hpp>
#include <emilua/byte_span.hpp>
#include <emilua/unix.hpp>

namespace emilua {

// from bytecode/ip.lua
extern unsigned char connect_bytecode[];
extern std::size_t connect_bytecode_size;
extern unsigned char data_op_bytecode[];
extern std::size_t data_op_bytecode_size;
extern unsigned char accept_bytecode[];
extern std::size_t accept_bytecode_size;

char unix_key;
char unix_datagram_socket_mt_key;
char unix_stream_acceptor_mt_key;
char unix_stream_socket_mt_key;

static char unix_datagram_socket_receive_key;
static char unix_datagram_socket_send_key;
static char unix_datagram_socket_send_to_key;
static char unix_stream_acceptor_accept_key;
static char unix_stream_socket_connect_key;
static char unix_stream_socket_read_some_key;
static char unix_stream_socket_write_some_key;

static int unix_datagram_socket_open(lua_State* L)
{
    auto sock = reinterpret_cast<unix_datagram_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_datagram_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    sock->socket.open(asio::local::datagram_protocol{}, ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int unix_datagram_socket_bind(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);
    auto sock = reinterpret_cast<unix_datagram_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_datagram_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    sock->socket.bind(tostringview(L, 2), ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int unix_datagram_socket_connect(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);
    auto sock = reinterpret_cast<unix_datagram_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_datagram_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    sock->socket.connect(tostringview(L, 2), ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int unix_datagram_socket_close(lua_State* L)
{
    auto sock = reinterpret_cast<unix_datagram_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_datagram_socket_mt_key);
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

static int unix_datagram_socket_cancel(lua_State* L)
{
    auto sock = reinterpret_cast<unix_datagram_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_datagram_socket_mt_key);
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

static int unix_datagram_socket_set_option(lua_State* L)
{
    lua_settop(L, 3);
    luaL_checktype(L, 2, LUA_TSTRING);

    auto socket = reinterpret_cast<unix_datagram_socket*>(lua_touserdata(L, 1));
    if (!socket || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_datagram_socket_mt_key);
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

static int unix_datagram_socket_get_option(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);

    auto socket = reinterpret_cast<unix_datagram_socket*>(lua_touserdata(L, 1));
    if (!socket || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_datagram_socket_mt_key);
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
            )
        ),
        [L](std::string_view /*key*/) -> int {
            push(L, std::errc::not_supported);
            return lua_error(L);
        },
        tostringview(L, 2)
    );
}

static int unix_datagram_socket_receive(lua_State* L)
{
    lua_settop(L, 3);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto sock = reinterpret_cast<unix_datagram_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_datagram_socket_mt_key);
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
            auto s = reinterpret_cast<unix_datagram_socket*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            boost::system::error_code ignored_ec;
            s->socket.cancel(ignored_ec);
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    auto remote_sender =
        std::make_shared<asio::local::datagram_protocol::endpoint>();

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

                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(
                                ec, bytes_transferred, remote_sender->path())))
                );
            }
        )
    );

    return lua_yield(L, 0);
}

static int unix_datagram_socket_send(lua_State* L)
{
    lua_settop(L, 3);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto sock = reinterpret_cast<unix_datagram_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_datagram_socket_mt_key);
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
            auto s = reinterpret_cast<unix_datagram_socket*>(
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

static int unix_datagram_socket_send_to(lua_State* L)
{
    lua_settop(L, 4);
    luaL_checktype(L, 3, LUA_TSTRING);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto sock = reinterpret_cast<unix_datagram_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_datagram_socket_mt_key);
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
    switch (lua_type(L, 4)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 4);
        return lua_error(L);
    case LUA_TNIL:
        flags = 0;
        break;
    case LUA_TNUMBER:
        flags = lua_tointeger(L, 4);
    }

    lua_pushvalue(L, 1);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto s = reinterpret_cast<unix_datagram_socket*>(
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
        asio::local::datagram_protocol::endpoint{tostringview(L, 3)},
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

static int unix_datagram_socket_io_control(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);

    auto socket = reinterpret_cast<unix_datagram_socket*>(lua_touserdata(L, 1));
    if (!socket || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_datagram_socket_mt_key);
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

inline int unix_datagram_socket_is_open(lua_State* L)
{
    auto sock = reinterpret_cast<unix_datagram_socket*>(lua_touserdata(L, 1));
    lua_pushboolean(L, sock->socket.is_open());
    return 1;
}

inline int unix_datagram_socket_local_path(lua_State* L)
{
    auto sock = reinterpret_cast<unix_datagram_socket*>(lua_touserdata(L, 1));
    boost::system::error_code ec;
    auto ep = sock->socket.local_endpoint(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    push(L, ep.path());
    return 1;
}

inline int unix_datagram_socket_remote_path(lua_State* L)
{
    auto sock = reinterpret_cast<unix_datagram_socket*>(lua_touserdata(L, 1));
    boost::system::error_code ec;
    auto ep = sock->socket.remote_endpoint(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    push(L, ep.path());
    return 1;
}

static int unix_datagram_socket_mt_index(lua_State* L)
{
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("open"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_datagram_socket_open);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("bind"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_datagram_socket_bind);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("connect"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_datagram_socket_connect);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("close"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_datagram_socket_close);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("cancel"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_datagram_socket_cancel);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("set_option"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_datagram_socket_set_option);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("get_option"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_datagram_socket_get_option);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("receive"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX,
                            &unix_datagram_socket_receive_key);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("send"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX,
                            &unix_datagram_socket_send_key);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("send_to"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX,
                            &unix_datagram_socket_send_to_key);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("io_control"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_datagram_socket_io_control);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("is_open"), unix_datagram_socket_is_open),
            hana::make_pair(
                BOOST_HANA_STRING("local_path"),
                unix_datagram_socket_local_path),
            hana::make_pair(
                BOOST_HANA_STRING("remote_path"),
                unix_datagram_socket_remote_path)
        ),
        [](std::string_view /*key*/, lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        },
        tostringview(L, 2),
        L
    );
}

static int unix_datagram_socket_new(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);
    auto sock = reinterpret_cast<unix_datagram_socket*>(
        lua_newuserdata(L, sizeof(unix_datagram_socket))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &unix_datagram_socket_mt_key);
    setmetatable(L, -2);
    new (sock) unix_datagram_socket{vm_ctx.strand().context()};
    return 1;
}

static int unix_datagram_socket_pair(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);

    auto sock1 = reinterpret_cast<unix_datagram_socket*>(
        lua_newuserdata(L, sizeof(unix_datagram_socket))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &unix_datagram_socket_mt_key);
    setmetatable(L, -2);
    new (sock1) unix_datagram_socket{vm_ctx.strand().context()};

    auto sock2 = reinterpret_cast<unix_datagram_socket*>(
        lua_newuserdata(L, sizeof(unix_datagram_socket))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &unix_datagram_socket_mt_key);
    setmetatable(L, -2);
    new (sock2) unix_datagram_socket{vm_ctx.strand().context()};

    boost::system::error_code ec;
    asio::local::connect_pair(sock1->socket, sock2->socket, ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 2;
}

static int unix_stream_socket_open(lua_State* L)
{
    auto sock = reinterpret_cast<unix_stream_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_stream_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    sock->socket.open(asio::local::stream_protocol{}, ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int unix_stream_socket_bind(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);
    auto sock = reinterpret_cast<unix_stream_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_stream_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    sock->socket.bind(tostringview(L, 2), ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int unix_stream_socket_close(lua_State* L)
{
    auto sock = reinterpret_cast<unix_stream_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_stream_socket_mt_key);
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

static int unix_stream_socket_cancel(lua_State* L)
{
    auto sock = reinterpret_cast<unix_stream_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_stream_socket_mt_key);
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

static int unix_stream_socket_io_control(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);

    auto socket = reinterpret_cast<unix_stream_socket*>(lua_touserdata(L, 1));
    if (!socket || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_stream_socket_mt_key);
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

static int unix_stream_socket_shutdown(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);

    auto socket = reinterpret_cast<unix_stream_socket*>(lua_touserdata(L, 1));
    if (!socket || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_stream_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    std::error_code ec;
    asio::local::stream_protocol::socket::shutdown_type what;
    dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("receive"),
                [&]() {
                    what =
                        asio::local::stream_protocol::socket::shutdown_receive;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("send"),
                [&]() {
                    what = asio::local::stream_protocol::socket::shutdown_send;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("both"),
                [&]() {
                    what = asio::local::stream_protocol::socket::shutdown_both;
                }
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

static int unix_stream_socket_connect(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);
    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto s = reinterpret_cast<unix_stream_socket*>(lua_touserdata(L, 1));
    if (!s || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_stream_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    asio::local::stream_protocol::endpoint ep{tostringview(L, 2)};

    lua_pushvalue(L, 1);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto s = reinterpret_cast<unix_stream_socket*>(
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

static int unix_stream_socket_read_some(lua_State* L)
{
    lua_settop(L, 2);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto s = reinterpret_cast<unix_stream_socket*>(lua_touserdata(L, 1));
    if (!s || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_stream_socket_mt_key);
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
            auto s = reinterpret_cast<unix_stream_socket*>(
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

static int unix_stream_socket_write_some(lua_State* L)
{
    lua_settop(L, 2);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto s = reinterpret_cast<unix_stream_socket*>(lua_touserdata(L, 1));
    if (!s || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_stream_socket_mt_key);
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
            auto s = reinterpret_cast<unix_stream_socket*>(
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

static int unix_stream_socket_set_option(lua_State* L)
{
    lua_settop(L, 3);
    luaL_checktype(L, 2, LUA_TSTRING);

    auto socket = reinterpret_cast<unix_stream_socket*>(lua_touserdata(L, 1));
    if (!socket || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_stream_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    return dispatch_table::dispatch(
        hana::make_tuple(
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
            )
        ),
        [L](std::string_view /*key*/) -> int {
            push(L, std::errc::not_supported);
            return lua_error(L);
        },
        tostringview(L, 2)
    );
}

static int unix_stream_socket_get_option(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);

    auto socket = reinterpret_cast<unix_stream_socket*>(lua_touserdata(L, 1));
    if (!socket || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_stream_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    return dispatch_table::dispatch(
        hana::make_tuple(
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
            )
        ),
        [L](std::string_view /*key*/) -> int {
            push(L, std::errc::not_supported);
            return lua_error(L);
        },
        tostringview(L, 2)
    );
}

inline int unix_stream_socket_is_open(lua_State* L)
{
    auto sock = reinterpret_cast<unix_stream_socket*>(lua_touserdata(L, 1));
    lua_pushboolean(L, sock->socket.is_open());
    return 1;
}

inline int unix_stream_socket_local_path(lua_State* L)
{
    auto sock = reinterpret_cast<unix_stream_socket*>(lua_touserdata(L, 1));
    boost::system::error_code ec;
    auto ep = sock->socket.local_endpoint(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    push(L, ep.path());
    return 1;
}

inline int unix_stream_socket_remote_path(lua_State* L)
{
    auto sock = reinterpret_cast<unix_stream_socket*>(lua_touserdata(L, 1));
    boost::system::error_code ec;
    auto ep = sock->socket.remote_endpoint(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    push(L, ep.path());
    return 1;
}

static int unix_stream_socket_mt_index(lua_State* L)
{
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("open"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_stream_socket_open);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("bind"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_stream_socket_bind);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("close"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_stream_socket_close);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("cancel"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_stream_socket_cancel);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("io_control"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_stream_socket_io_control);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("shutdown"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_stream_socket_shutdown);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("connect"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX,
                            &unix_stream_socket_connect_key);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("read_some"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX,
                            &unix_stream_socket_read_some_key);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("write_some"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX,
                            &unix_stream_socket_write_some_key);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("set_option"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_stream_socket_set_option);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("get_option"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_stream_socket_get_option);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("is_open"), unix_stream_socket_is_open),
            hana::make_pair(
                BOOST_HANA_STRING("local_path"), unix_stream_socket_local_path),
            hana::make_pair(
                BOOST_HANA_STRING("remote_path"),
                unix_stream_socket_remote_path)
        ),
        [](std::string_view /*key*/, lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        },
        tostringview(L, 2),
        L
    );
}

static int unix_stream_socket_new(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);
    auto sock = reinterpret_cast<unix_stream_socket*>(
        lua_newuserdata(L, sizeof(unix_stream_socket))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &unix_stream_socket_mt_key);
    setmetatable(L, -2);
    new (sock) unix_stream_socket{vm_ctx.strand().context()};
    return 1;
}

static int unix_stream_socket_pair(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);

    auto sock1 = reinterpret_cast<unix_stream_socket*>(
        lua_newuserdata(L, sizeof(unix_stream_socket))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &unix_stream_socket_mt_key);
    setmetatable(L, -2);
    new (sock1) unix_stream_socket{vm_ctx.strand().context()};

    auto sock2 = reinterpret_cast<unix_stream_socket*>(
        lua_newuserdata(L, sizeof(unix_stream_socket))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &unix_stream_socket_mt_key);
    setmetatable(L, -2);
    new (sock2) unix_stream_socket{vm_ctx.strand().context()};

    boost::system::error_code ec;
    asio::local::connect_pair(sock1->socket, sock2->socket, ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 2;
}

static int unix_stream_acceptor_open(lua_State* L)
{
    auto acceptor = reinterpret_cast<asio::local::stream_protocol::acceptor*>(
        lua_touserdata(L, 1));
    if (!acceptor || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_stream_acceptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    acceptor->open(asio::local::stream_protocol{}, ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int unix_stream_acceptor_bind(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);
    auto acceptor = reinterpret_cast<asio::local::stream_protocol::acceptor*>(
        lua_touserdata(L, 1));
    if (!acceptor || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_stream_acceptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    acceptor->bind(tostringview(L, 2), ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int unix_stream_acceptor_listen(lua_State* L)
{
    lua_settop(L, 2);
    auto acceptor = reinterpret_cast<asio::local::stream_protocol::acceptor*>(
        lua_touserdata(L, 1));
    if (!acceptor || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_stream_acceptor_mt_key);
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

static int unix_stream_acceptor_accept(lua_State* L)
{
    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto acceptor = reinterpret_cast<asio::local::stream_protocol::acceptor*>(
        lua_touserdata(L, 1));
    if (!acceptor || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_stream_acceptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    lua_pushvalue(L, 1);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto a = reinterpret_cast<asio::local::stream_protocol::acceptor*>(
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
                               asio::local::stream_protocol::socket peer) {
            auto peer_pusher = [&ec,&peer](lua_State* fiber) {
                if (ec) {
                    lua_pushnil(fiber);
                } else {
                    auto s = reinterpret_cast<unix_stream_socket*>(
                        lua_newuserdata(fiber, sizeof(unix_stream_socket)));
                    rawgetp(fiber, LUA_REGISTRYINDEX,
                            &unix_stream_socket_mt_key);
                    setmetatable(fiber, -2);
                    new (s) unix_stream_socket{std::move(peer)};
                }
            };

            vm_ctx->fiber_resume(
                current_fiber,
                hana::make_set(
                    vm_context::options::auto_detect_interrupt,
                    hana::make_pair(
                        vm_context::options::arguments,
                        hana::make_tuple(ec, peer_pusher))));
        }
    ));

    return lua_yield(L, 0);
}

static int unix_stream_acceptor_close(lua_State* L)
{
    auto acceptor = reinterpret_cast<asio::local::stream_protocol::acceptor*>(
        lua_touserdata(L, 1));
    if (!acceptor || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_stream_acceptor_mt_key);
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

static int unix_stream_acceptor_cancel(lua_State* L)
{
    auto acceptor = reinterpret_cast<asio::local::stream_protocol::acceptor*>(
        lua_touserdata(L, 1));
    if (!acceptor || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_stream_acceptor_mt_key);
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

static int unix_stream_acceptor_set_option(lua_State* L)
{
    auto acceptor = reinterpret_cast<asio::local::stream_protocol::acceptor*>(
        lua_touserdata(L, 1));
    if (!acceptor || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_stream_acceptor_mt_key);
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
                BOOST_HANA_STRING("enable_connection_aborted"),
                [&]() -> int {
                    luaL_checktype(L, 3, LUA_TBOOLEAN);
                    asio::socket_base::enable_connection_aborted o(
                        lua_toboolean(L, 3));
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
    return 0;
}

static int unix_stream_acceptor_get_option(lua_State* L)
{
    auto acceptor = reinterpret_cast<asio::local::stream_protocol::acceptor*>(
        lua_touserdata(L, 1));
    if (!acceptor || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &unix_stream_acceptor_mt_key);
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
            )
        ),
        [L](std::string_view /*key*/) -> int {
            push(L, std::errc::not_supported);
            return lua_error(L);
        },
        tostringview(L, 2)
    );
}

inline int unix_stream_acceptor_is_open(lua_State* L)
{
    auto acceptor = reinterpret_cast<asio::local::stream_protocol::acceptor*>(
        lua_touserdata(L, 1));
    lua_pushboolean(L, acceptor->is_open());
    return 1;
}

inline int unix_stream_acceptor_local_path(lua_State* L)
{
    auto acceptor = reinterpret_cast<asio::local::stream_protocol::acceptor*>(
        lua_touserdata(L, 1));
    boost::system::error_code ec;
    auto ep = acceptor->local_endpoint(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    push(L, ep.path());
    return 1;
}

static int unix_stream_acceptor_mt_index(lua_State* L)
{
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("open"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_stream_acceptor_open);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("bind"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_stream_acceptor_bind);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("listen"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_stream_acceptor_listen);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("accept"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX,
                            &unix_stream_acceptor_accept_key);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("close"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_stream_acceptor_close);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("cancel"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_stream_acceptor_cancel);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("set_option"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_stream_acceptor_set_option);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("get_option"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, unix_stream_acceptor_get_option);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("is_open"), unix_stream_acceptor_is_open),
            hana::make_pair(
                BOOST_HANA_STRING("local_path"),
                unix_stream_acceptor_local_path)
        ),
        [](std::string_view /*key*/, lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        },
        tostringview(L, 2),
        L
    );
}

static int unix_stream_acceptor_new(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);
    auto s = reinterpret_cast<asio::local::stream_protocol::acceptor*>(
        lua_newuserdata(L, sizeof(asio::local::stream_protocol::acceptor))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &unix_stream_acceptor_mt_key);
    setmetatable(L, -2);
    new (s) asio::local::stream_protocol::acceptor{vm_ctx.strand().context()};
    return 1;
}

void init_unix(lua_State* L)
{
    lua_pushlightuserdata(L, &unix_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "message_flag");
        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/1);

            lua_pushliteral(L, "peek");
            lua_pushinteger(L, asio::socket_base::message_peek);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);

        lua_pushliteral(L, "datagram_protocol");
        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/1);

            lua_pushliteral(L, "socket");
            {
                lua_createtable(L, /*narr=*/0, /*nrec=*/1);

                lua_pushliteral(L, "new");
                lua_pushcfunction(L, unix_datagram_socket_new);
                lua_rawset(L, -3);

                lua_pushliteral(L, "pair");
                lua_pushcfunction(L, unix_datagram_socket_pair);
                lua_rawset(L, -3);
            }
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);

        lua_pushliteral(L, "stream_protocol");
        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/2);

            lua_pushliteral(L, "socket");
            {
                lua_createtable(L, /*narr=*/0, /*nrec=*/2);

                lua_pushliteral(L, "new");
                lua_pushcfunction(L, unix_stream_socket_new);
                lua_rawset(L, -3);

                lua_pushliteral(L, "pair");
                lua_pushcfunction(L, unix_stream_socket_pair);
                lua_rawset(L, -3);
            }
            lua_rawset(L, -3);

            lua_pushliteral(L, "acceptor");
            {
                lua_createtable(L, /*narr=*/0, /*nrec=*/1);

                lua_pushliteral(L, "new");
                lua_pushcfunction(L, unix_stream_acceptor_new);
                lua_rawset(L, -3);
            }
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &unix_datagram_socket_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "unix.datagram_protocol.socket");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, unix_datagram_socket_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<unix_datagram_socket>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &unix_stream_socket_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "unix.stream_protocol.socket");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, unix_stream_socket_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<unix_stream_socket>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &unix_stream_acceptor_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "unix.stream_protocol.acceptor");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, unix_stream_acceptor_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<asio::local::stream_protocol::acceptor>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &unix_datagram_socket_receive_key);
    int res = luaL_loadbuffer(L, reinterpret_cast<char*>(data_op_bytecode),
                              data_op_bytecode_size, nullptr);
    assert(res == 0); boost::ignore_unused(res);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, unix_datagram_socket_receive);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &unix_datagram_socket_send_key);
    res = luaL_loadbuffer(L, reinterpret_cast<char*>(data_op_bytecode),
                          data_op_bytecode_size, nullptr);
    assert(res == 0); boost::ignore_unused(res);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, unix_datagram_socket_send);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &unix_datagram_socket_send_to_key);
    res = luaL_loadbuffer(L, reinterpret_cast<char*>(data_op_bytecode),
                          data_op_bytecode_size, nullptr);
    assert(res == 0); boost::ignore_unused(res);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, unix_datagram_socket_send_to);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &unix_stream_socket_connect_key);
    res = luaL_loadbuffer(L, reinterpret_cast<char*>(connect_bytecode),
                          connect_bytecode_size, nullptr);
    assert(res == 0); boost::ignore_unused(res);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, unix_stream_socket_connect);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &unix_stream_socket_read_some_key);
    res = luaL_loadbuffer(L, reinterpret_cast<char*>(data_op_bytecode),
                          data_op_bytecode_size, nullptr);
    assert(res == 0); boost::ignore_unused(res);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, unix_stream_socket_read_some);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &unix_stream_socket_write_some_key);
    res = luaL_loadbuffer(L, reinterpret_cast<char*>(data_op_bytecode),
                          data_op_bytecode_size, nullptr);
    assert(res == 0); boost::ignore_unused(res);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, unix_stream_socket_write_some);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &unix_stream_acceptor_accept_key);
    res = luaL_loadbuffer(L, reinterpret_cast<char*>(accept_bytecode),
                          accept_bytecode_size, nullptr);
    assert(res == 0); boost::ignore_unused(res);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, unix_stream_acceptor_accept);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);
}

} // namespace emilua
