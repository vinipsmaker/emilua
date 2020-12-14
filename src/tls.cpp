/* Copyright (c) 2020 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#include <boost/asio/ssl.hpp>

#include <emilua/dispatch_table.hpp>
#include <emilua/fiber.hpp>
#include <emilua/tls.hpp>
#include <emilua/ip.hpp>

namespace emilua {

extern unsigned char handshake_bytecode[];
extern std::size_t handshake_bytecode_size;

char tls_key;
char tls_ctx_mt_key;
char tls_socket_mt_key;

static char socket_client_handshake_key;
static char socket_server_handshake_key;

static int tls_ctx_new(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TSTRING);
    std::optional<asio::ssl::context::method> method;
    dispatch_table::dispatch(
        hana::make_tuple(
#define EMILUA_DEFINE_METHOD_FLAG(M)                \
            hana::make_pair(                        \
                BOOST_HANA_STRING(#M),              \
                [&method]() {                       \
                    method = asio::ssl::context::M; \
                }                                   \
            )
            EMILUA_DEFINE_METHOD_FLAG(sslv2),
            EMILUA_DEFINE_METHOD_FLAG(sslv2_client),
            EMILUA_DEFINE_METHOD_FLAG(sslv2_server),
            EMILUA_DEFINE_METHOD_FLAG(sslv3),
            EMILUA_DEFINE_METHOD_FLAG(sslv3_client),
            EMILUA_DEFINE_METHOD_FLAG(sslv3_server),
            EMILUA_DEFINE_METHOD_FLAG(tlsv1),
            EMILUA_DEFINE_METHOD_FLAG(tlsv1_client),
            EMILUA_DEFINE_METHOD_FLAG(tlsv1_server),
            EMILUA_DEFINE_METHOD_FLAG(sslv23),
            EMILUA_DEFINE_METHOD_FLAG(sslv23_client),
            EMILUA_DEFINE_METHOD_FLAG(sslv23_server),
            EMILUA_DEFINE_METHOD_FLAG(tlsv11),
            EMILUA_DEFINE_METHOD_FLAG(tlsv11_client),
            EMILUA_DEFINE_METHOD_FLAG(tlsv11_server),
            EMILUA_DEFINE_METHOD_FLAG(tlsv12),
            EMILUA_DEFINE_METHOD_FLAG(tlsv12_client),
            EMILUA_DEFINE_METHOD_FLAG(tlsv12_server),
            EMILUA_DEFINE_METHOD_FLAG(tlsv13),
            EMILUA_DEFINE_METHOD_FLAG(tlsv13_client),
            EMILUA_DEFINE_METHOD_FLAG(tlsv13_server),
            EMILUA_DEFINE_METHOD_FLAG(tls),
            EMILUA_DEFINE_METHOD_FLAG(tls_client),
            EMILUA_DEFINE_METHOD_FLAG(tls_server)
#undef EMILUA_DEFINE_METHOD_FLAG
        ),
        [](std::string_view /*key*/) {},
        tostringview(L, 1)
    );
    if (!method) {
        push(L, std::errc::invalid_argument);
        lua_pushliteral(L, "arg");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }
    try {
        auto ctx = std::make_shared<asio::ssl::context>(*method);

        auto c = reinterpret_cast<std::shared_ptr<asio::ssl::context>*>(
            lua_newuserdata(L, sizeof(std::shared_ptr<asio::ssl::context>))
        );
        rawgetp(L, LUA_REGISTRYINDEX, &tls_ctx_mt_key);
        int res = lua_setmetatable(L, -2);
        assert(res); boost::ignore_unused(res);
        new (c) std::shared_ptr<asio::ssl::context>{std::move(ctx)};
        return 1;
    } catch (const boost::system::system_error& e) {
        push(L, static_cast<std::error_code>(e.code()));
        return lua_error(L);
    }
}

static int tls_socket_new(lua_State* L)
{
    lua_settop(L, 2);

    auto tcp_s = reinterpret_cast<asio::ip::tcp::socket*>(lua_touserdata(L, 1));
    if (!tcp_s || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument);
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument);
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }

    auto c = reinterpret_cast<std::shared_ptr<asio::ssl::context>*>(
        lua_touserdata(L, 2)
    );
    if (!c || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument);
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 2);
        lua_rawset(L, -3);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &tls_ctx_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument);
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 2);
        lua_rawset(L, -3);
        return lua_error(L);
    }

    auto s = reinterpret_cast<TlsSocket*>(
        lua_newuserdata(L, sizeof(TlsSocket))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &tls_socket_mt_key);
    int res = lua_setmetatable(L, -2);
    assert(res); boost::ignore_unused(res);
    new (s) TlsSocket{*tcp_s, *c};

    lua_pushnil(L);
    res = lua_setmetatable(L, 1);
    assert(res); boost::ignore_unused(res);
    tcp_s->asio::ip::tcp::socket::~socket();

    return 1;
}

template<asio::ssl::stream_base::handshake_type HANDSHAKE>
static int socket_handshake(lua_State* L)
{
    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto s = reinterpret_cast<TlsSocket*>(lua_touserdata(L, 1));
    if (!s || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument);
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &tls_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument);
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }

    lua_pushvalue(L, 1);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto s = reinterpret_cast<TlsSocket*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            boost::system::error_code ignored_ec;
            s->socket.next_layer().cancel(ignored_ec);
            return 0;
        },
        1);
    set_interrupter(L);

    s->socket.async_handshake(HANDSHAKE, asio::bind_executor(
        vm_ctx->strand_using_defer(),
        [vm_ctx,current_fiber](const boost::system::error_code& ec) {
            std::error_code std_ec = ec;
            vm_ctx->fiber_prologue(
                current_fiber,
                [&]() {
                    if (ec == asio::error::operation_aborted) {
                        rawgetp(current_fiber, LUA_REGISTRYINDEX,
                                &fiber_list_key);
                        lua_pushthread(current_fiber);
                        lua_rawget(current_fiber, -2);
                        lua_rawgeti(current_fiber, -1,
                                    FiberDataIndex::INTERRUPTED);
                        bool interrupted = lua_toboolean(current_fiber, -1);
                        lua_pop(current_fiber, 3);
                        if (interrupted)
                            std_ec = errc::interrupted;
                    }
                    push(current_fiber, std_ec);
                });
            int res = lua_resume(current_fiber, 1);
            vm_ctx->fiber_epilogue(res);
        }
    ));

    return lua_yield(L, 0);
}

static int tls_socket_meta_index(lua_State* L)
{
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("client_handshake"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX, &socket_client_handshake_key);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("server_handshake"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX, &socket_server_handshake_key);
                    return 1;
                }
            )
        ),
        [](std::string_view /*key*/, lua_State* L) -> int {
            push(L, errc::bad_index);
            lua_pushliteral(L, "index");
            lua_pushvalue(L, 2);
            lua_rawset(L, -3);
            return lua_error(L);
        },
        tostringview(L, 2),
        L
    );
}

void init_tls(lua_State* L)
{
    lua_pushlightuserdata(L, &tls_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/2);

        lua_pushliteral(L, "ctx");
        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/1);

            lua_pushliteral(L, "new");
            lua_pushcfunction(L, tls_ctx_new);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);

        lua_pushliteral(L, "socket");
        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/1);

            lua_pushliteral(L, "new");
            lua_pushcfunction(L, tls_socket_new);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &tls_ctx_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/2);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "tls.ctx");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<std::shared_ptr<asio::ssl::context>>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &tls_socket_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "tls.socket");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, tls_socket_meta_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<TlsSocket>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    int res = luaL_loadbuffer(L, reinterpret_cast<char*>(handshake_bytecode),
                              handshake_bytecode_size, nullptr);
    assert(res == 0); boost::ignore_unused(res);

    lua_pushlightuserdata(L, &socket_client_handshake_key);
    lua_pushvalue(L, -2);
    lua_pushcfunction(L, lua_error);
    lua_pushcfunction(L, socket_handshake<asio::ssl::stream_base::client>);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &socket_server_handshake_key);
    lua_insert(L, -2);
    lua_pushcfunction(L, lua_error);
    lua_pushcfunction(L, socket_handshake<asio::ssl::stream_base::server>);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);
}

} // namespace emilua
