/* Copyright (c) 2020, 2022 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#include <optional>

#include <boost/smart_ptr/local_shared_ptr.hpp>
#include <boost/scope_exit.hpp>
#include <boost/asio/ssl.hpp>

#include <emilua/dispatch_table.hpp>
#include <emilua/async_base.hpp>
#include <emilua/byte_span.hpp>
#include <emilua/tls.hpp>
#include <emilua/ip.hpp>

namespace emilua {

char tls_key;
char tls_context_mt_key;
char tls_socket_mt_key;

static char socket_client_handshake_key;
static char socket_server_handshake_key;
static char tls_socket_read_some_key;
static char tls_socket_write_some_key;

struct context_password_callback
{
    struct resource
    {
        resource(vm_context& vm_ctx, int ref)
            : vm_ctx{vm_ctx.weak_from_this()}
            , ref{ref}
        {
            assert(ref != LUA_NOREF);
        }

        ~resource()
        {
            auto vm_ctx = this->vm_ctx.lock();
            if (!vm_ctx)
                return;

            auto executor = vm_ctx->strand();
            executor.dispatch([ref=this->ref,vm_ctx]() {
                if (!vm_ctx->valid())
                    return;

                lua_State* L = vm_ctx->async_event_thread();
                luaL_unref(L, LUA_REGISTRYINDEX, ref);
            });
        }

        std::weak_ptr<vm_context> vm_ctx;
        int ref;
    };

    context_password_callback(vm_context& vm_ctx, int ref)
        : shared_resource{new resource{vm_ctx, ref}}
    {}

    std::string operator()(std::size_t max_length,
                           asio::ssl::context::password_purpose purpose)
    {
        auto vm_ctx = shared_resource->vm_ctx.lock();
        if (!vm_ctx)
            return {};

        assert(vm_ctx->strand().running_in_this_thread());
        if (!vm_ctx->valid())
            return {};

        lua_State* L = vm_ctx->async_event_thread();
        lua_rawgeti(L, LUA_REGISTRYINDEX, shared_resource->ref);
        lua_pushinteger(L, max_length);
        try {
            switch (purpose) {
            case asio::ssl::context::for_reading:
                lua_pushliteral(L, "for_reading");
                break;
            case asio::ssl::context::for_writing:
                lua_pushliteral(L, "for_writing");
            }
        } catch (...) {
            vm_ctx->notify_errmem();
            vm_ctx->close();
            return {};
        }
        int res = lua_pcall(L, 2, 1, 0);
        if (res == LUA_ERRMEM) {
            vm_ctx->notify_errmem();
            vm_ctx->close();
            return {};
        }

        BOOST_SCOPE_EXIT_ALL(&) { lua_settop(L, 0); };
        switch (res) {
        case 0:
            if (lua_type(L, -1) != LUA_TSTRING) {
        default:
                return {};
            }

            return static_cast<std::string>(tostringview(L));
        }
    }

    boost::local_shared_ptr<resource> shared_resource;
};

static int tls_context_new(lua_State* L)
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
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    try {
        auto ctx = std::make_shared<asio::ssl::context>(*method);

        auto c = reinterpret_cast<std::shared_ptr<asio::ssl::context>*>(
            lua_newuserdata(L, sizeof(std::shared_ptr<asio::ssl::context>))
        );
        rawgetp(L, LUA_REGISTRYINDEX, &tls_context_mt_key);
        setmetatable(L, -2);
        new (c) std::shared_ptr<asio::ssl::context>{std::move(ctx)};
        return 1;
    } catch (const boost::system::system_error& e) {
        push(L, static_cast<std::error_code>(e.code()));
        return lua_error(L);
    }
}

static int context_add_certificate_authority(lua_State* L)
{
    auto ctx = reinterpret_cast<std::shared_ptr<asio::ssl::context>*>(
        lua_touserdata(L, 1)
    );
    if (!ctx || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &tls_context_mt_key);
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

    boost::system::error_code ec;
    (*ctx)->add_certificate_authority(
        asio::buffer(bs->data.get(), bs->size), ec);
    if (ec) {
        push(L, ec);
        return lua_error(L);
    }
    return 0;
}

static int context_add_verify_path(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);

    auto ctx = reinterpret_cast<std::shared_ptr<asio::ssl::context>*>(
        lua_touserdata(L, 1)
    );
    if (!ctx || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &tls_context_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    (*ctx)->add_verify_path(static_cast<std::string>(tostringview(L, 2)), ec);
    if (ec) {
        push(L, ec);
        return lua_error(L);
    }
    return 0;
}

static int context_clear_options(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TNUMBER);

    auto ctx = reinterpret_cast<std::shared_ptr<asio::ssl::context>*>(
        lua_touserdata(L, 1)
    );
    if (!ctx || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &tls_context_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    (*ctx)->clear_options(lua_tointeger(L, 2), ec);
    if (ec) {
        push(L, ec);
        return lua_error(L);
    }
    return 0;
}

static int context_load_verify_file(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);

    auto ctx = reinterpret_cast<std::shared_ptr<asio::ssl::context>*>(
        lua_touserdata(L, 1)
    );
    if (!ctx || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &tls_context_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    (*ctx)->load_verify_file(static_cast<std::string>(tostringview(L, 2)), ec);
    if (ec) {
        push(L, ec);
        return lua_error(L);
    }
    return 0;
}

static int context_set_default_verify_paths(lua_State* L)
{
    auto ctx = reinterpret_cast<std::shared_ptr<asio::ssl::context>*>(
        lua_touserdata(L, 1)
    );
    if (!ctx || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &tls_context_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    (*ctx)->set_default_verify_paths(ec);
    if (ec) {
        push(L, ec);
        return lua_error(L);
    }
    return 0;
}

static int context_set_options(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TNUMBER);

    auto ctx = reinterpret_cast<std::shared_ptr<asio::ssl::context>*>(
        lua_touserdata(L, 1)
    );
    if (!ctx || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &tls_context_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    (*ctx)->set_options(lua_tointeger(L, 2), ec);
    if (ec) {
        push(L, ec);
        return lua_error(L);
    }
    return 0;
}

static int context_set_password_callback(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TFUNCTION);

    auto& vm_ctx = get_vm_context(L);

    auto ctx = reinterpret_cast<std::shared_ptr<asio::ssl::context>*>(
        lua_touserdata(L, 1)
    );
    if (!ctx || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &tls_context_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    lua_pushvalue(L, 2);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);

    boost::system::error_code ec;
    (*ctx)->set_password_callback(context_password_callback{vm_ctx, ref}, ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int context_set_verify_callback(lua_State* L)
{
    lua_settop(L, 3);
    luaL_checktype(L, 2, LUA_TSTRING);

    auto ctx = reinterpret_cast<std::shared_ptr<asio::ssl::context>*>(
        lua_touserdata(L, 1)
    );
    if (!ctx || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &tls_context_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("host_name_verification"),
                [&]() -> int {
                    luaL_checktype(L, 3, LUA_TSTRING);
                    asio::ssl::host_name_verification o{
                        static_cast<std::string>(tostringview(L, 3))};
                    (*ctx)->set_verify_callback(std::move(o), ec);
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

static int context_set_verify_depth(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TNUMBER);

    auto ctx = reinterpret_cast<std::shared_ptr<asio::ssl::context>*>(
        lua_touserdata(L, 1)
    );
    if (!ctx || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &tls_context_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    (*ctx)->set_verify_depth(lua_tointeger(L, 2), ec);
    if (ec) {
        push(L, ec);
        return lua_error(L);
    }
    return 0;
}

static int context_set_verify_mode(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);

    auto ctx = reinterpret_cast<std::shared_ptr<asio::ssl::context>*>(
        lua_touserdata(L, 1)
    );
    if (!ctx || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &tls_context_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("none"),
                [&]() -> int {
                    (*ctx)->set_verify_mode(asio::ssl::verify_none, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("peer"),
                [&]() -> int {
                    (*ctx)->set_verify_mode(asio::ssl::verify_peer, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("fail_if_no_peer_cert"),
                [&]() -> int {
                    (*ctx)->set_verify_mode(
                        asio::ssl::verify_fail_if_no_peer_cert, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("client_once"),
                [&]() -> int {
                    (*ctx)->set_verify_mode(asio::ssl::verify_client_once, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            )
        ),
        [L](std::string_view /*key*/) -> int {
            push(L, std::errc::invalid_argument);
            return lua_error(L);
        },
        tostringview(L, 2)
    );
    return 0;
}

static int context_use_certificate(lua_State* L)
{
    luaL_checktype(L, 3, LUA_TSTRING);

    auto ctx = reinterpret_cast<std::shared_ptr<asio::ssl::context>*>(
        lua_touserdata(L, 1)
    );
    if (!ctx || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &tls_context_mt_key);
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

    asio::ssl::context::file_format fmt;
    auto fmt_str = tostringview(L, 3);
    if (fmt_str == "pem") {
        fmt = asio::ssl::context::pem;
    } else if (fmt_str == "asn1") {
        fmt = asio::ssl::context::asn1;
    } else {
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    }

    boost::system::error_code ec;
    (*ctx)->use_certificate(asio::buffer(bs->data.get(), bs->size), fmt, ec);
    if (ec) {
        push(L, ec);
        return lua_error(L);
    }
    return 0;
}

static int context_use_certificate_chain(lua_State* L)
{
    auto ctx = reinterpret_cast<std::shared_ptr<asio::ssl::context>*>(
        lua_touserdata(L, 1)
    );
    if (!ctx || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &tls_context_mt_key);
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

    boost::system::error_code ec;
    (*ctx)->use_certificate_chain(asio::buffer(bs->data.get(), bs->size), ec);
    if (ec) {
        push(L, ec);
        return lua_error(L);
    }
    return 0;
}

static int context_use_certificate_chain_file(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);

    auto ctx = reinterpret_cast<std::shared_ptr<asio::ssl::context>*>(
        lua_touserdata(L, 1)
    );
    if (!ctx || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &tls_context_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    (*ctx)->use_certificate_chain_file(
        static_cast<std::string>(tostringview(L, 2)), ec);
    if (ec) {
        push(L, ec);
        return lua_error(L);
    }
    return 0;
}

static int context_use_certificate_file(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);
    luaL_checktype(L, 3, LUA_TSTRING);

    auto ctx = reinterpret_cast<std::shared_ptr<asio::ssl::context>*>(
        lua_touserdata(L, 1)
    );
    if (!ctx || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &tls_context_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    asio::ssl::context::file_format fmt;
    auto fmt_str = tostringview(L, 3);
    if (fmt_str == "pem") {
        fmt = asio::ssl::context::pem;
    } else if (fmt_str == "asn1") {
        fmt = asio::ssl::context::asn1;
    } else {
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    }

    boost::system::error_code ec;
    (*ctx)->use_certificate_file(
        static_cast<std::string>(tostringview(L, 2)), fmt, ec);
    if (ec) {
        push(L, ec);
        return lua_error(L);
    }
    return 0;
}

static int context_use_private_key(lua_State* L)
{
    luaL_checktype(L, 3, LUA_TSTRING);

    auto ctx = reinterpret_cast<std::shared_ptr<asio::ssl::context>*>(
        lua_touserdata(L, 1)
    );
    if (!ctx || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &tls_context_mt_key);
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

    asio::ssl::context::file_format fmt;
    auto fmt_str = tostringview(L, 3);
    if (fmt_str == "pem") {
        fmt = asio::ssl::context::pem;
    } else if (fmt_str == "asn1") {
        fmt = asio::ssl::context::asn1;
    } else {
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    }

    boost::system::error_code ec;
    (*ctx)->use_private_key(asio::buffer(bs->data.get(), bs->size), fmt, ec);
    if (ec) {
        push(L, ec);
        return lua_error(L);
    }
    return 0;
}

static int context_use_private_key_file(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);
    luaL_checktype(L, 3, LUA_TSTRING);

    auto ctx = reinterpret_cast<std::shared_ptr<asio::ssl::context>*>(
        lua_touserdata(L, 1)
    );
    if (!ctx || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &tls_context_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    asio::ssl::context::file_format fmt;
    auto fmt_str = tostringview(L, 3);
    if (fmt_str == "pem") {
        fmt = asio::ssl::context::pem;
    } else if (fmt_str == "asn1") {
        fmt = asio::ssl::context::asn1;
    } else {
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    }

    boost::system::error_code ec;
    (*ctx)->use_private_key_file(
        static_cast<std::string>(tostringview(L, 2)), fmt, ec);
    if (ec) {
        push(L, ec);
        return lua_error(L);
    }
    return 0;
}

static int context_use_rsa_private_key(lua_State* L)
{
    luaL_checktype(L, 3, LUA_TSTRING);

    auto ctx = reinterpret_cast<std::shared_ptr<asio::ssl::context>*>(
        lua_touserdata(L, 1)
    );
    if (!ctx || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &tls_context_mt_key);
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

    asio::ssl::context::file_format fmt;
    auto fmt_str = tostringview(L, 3);
    if (fmt_str == "pem") {
        fmt = asio::ssl::context::pem;
    } else if (fmt_str == "asn1") {
        fmt = asio::ssl::context::asn1;
    } else {
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    }

    boost::system::error_code ec;
    (*ctx)->use_rsa_private_key(
        asio::buffer(bs->data.get(), bs->size), fmt, ec);
    if (ec) {
        push(L, ec);
        return lua_error(L);
    }
    return 0;
}

static int context_use_rsa_private_key_file(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);
    luaL_checktype(L, 3, LUA_TSTRING);

    auto ctx = reinterpret_cast<std::shared_ptr<asio::ssl::context>*>(
        lua_touserdata(L, 1)
    );
    if (!ctx || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &tls_context_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    asio::ssl::context::file_format fmt;
    auto fmt_str = tostringview(L, 3);
    if (fmt_str == "pem") {
        fmt = asio::ssl::context::pem;
    } else if (fmt_str == "asn1") {
        fmt = asio::ssl::context::asn1;
    } else {
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    }

    boost::system::error_code ec;
    (*ctx)->use_rsa_private_key_file(
        static_cast<std::string>(tostringview(L, 2)), fmt, ec);
    if (ec) {
        push(L, ec);
        return lua_error(L);
    }
    return 0;
}

static int context_use_tmp_dh(lua_State* L)
{
    auto ctx = reinterpret_cast<std::shared_ptr<asio::ssl::context>*>(
        lua_touserdata(L, 1)
    );
    if (!ctx || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &tls_context_mt_key);
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

    boost::system::error_code ec;
    (*ctx)->use_tmp_dh(asio::buffer(bs->data.get(), bs->size), ec);
    if (ec) {
        push(L, ec);
        return lua_error(L);
    }
    return 0;
}

static int context_use_tmp_dh_file(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);

    auto ctx = reinterpret_cast<std::shared_ptr<asio::ssl::context>*>(
        lua_touserdata(L, 1)
    );
    if (!ctx || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &tls_context_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    (*ctx)->use_tmp_dh_file(static_cast<std::string>(tostringview(L, 2)), ec);
    if (ec) {
        push(L, ec);
        return lua_error(L);
    }
    return 0;
}

static int tls_context_mt_index(lua_State* L)
{
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("add_certificate_authority"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, context_add_certificate_authority);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("add_verify_path"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, context_add_verify_path);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("clear_options"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, context_clear_options);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("load_verify_file"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, context_load_verify_file);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("set_default_verify_paths"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, context_set_default_verify_paths);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("set_options"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, context_set_options);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("set_password_callback"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, context_set_password_callback);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("set_verify_callback"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, context_set_verify_callback);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("set_verify_depth"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, context_set_verify_depth);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("set_verify_mode"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, context_set_verify_mode);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("use_certificate"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, context_use_certificate);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("use_certificate_chain"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, context_use_certificate_chain);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("use_certificate_chain_file"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, context_use_certificate_chain_file);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("use_certificate_file"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, context_use_certificate_file);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("use_private_key"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, context_use_private_key);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("use_private_key_file"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, context_use_private_key_file);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("use_rsa_private_key"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, context_use_rsa_private_key);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("use_rsa_private_key_file"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, context_use_rsa_private_key_file);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("use_tmp_dh"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, context_use_tmp_dh);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("use_tmp_dh_file"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, context_use_tmp_dh_file);
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

static int tls_socket_new(lua_State* L)
{
    lua_settop(L, 2);

    auto tcp_s = reinterpret_cast<asio::ip::tcp::socket*>(lua_touserdata(L, 1));
    if (!tcp_s || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto c = reinterpret_cast<std::shared_ptr<asio::ssl::context>*>(
        lua_touserdata(L, 2)
    );
    if (!c || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &tls_context_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    auto s = reinterpret_cast<TlsSocket*>(
        lua_newuserdata(L, sizeof(TlsSocket))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &tls_socket_mt_key);
    setmetatable(L, -2);
    new (s) TlsSocket{*tcp_s, *c};

    lua_pushnil(L);
    setmetatable(L, 1);
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
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &tls_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    lua_pushvalue(L, 1);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto s = reinterpret_cast<TlsSocket*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            boost::system::error_code ignored_ec;
            s->next_layer().cancel(ignored_ec);
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    s->async_handshake(HANDSHAKE, asio::bind_executor(
        vm_ctx->strand_using_defer(),
        [vm_ctx,current_fiber](const boost::system::error_code& ec) {
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

static int tls_socket_read_some(lua_State* L)
{
    lua_settop(L, 2);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto s = reinterpret_cast<TlsSocket*>(lua_touserdata(L, 1));
    if (!s || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &tls_socket_mt_key);
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
            auto s = reinterpret_cast<TlsSocket*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            boost::system::error_code ignored_ec;
            s->next_layer().cancel(ignored_ec);
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    s->async_read_some(
        asio::buffer(bs->data.get(), bs->size),
        asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,buf=bs->data](
                const boost::system::error_code& ec,
                std::size_t bytes_transferred
            ) {
                boost::ignore_unused(buf);
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

static int tls_socket_write_some(lua_State* L)
{
    lua_settop(L, 2);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto s = reinterpret_cast<TlsSocket*>(lua_touserdata(L, 1));
    if (!s || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &tls_socket_mt_key);
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
            auto s = reinterpret_cast<TlsSocket*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            boost::system::error_code ignored_ec;
            s->next_layer().cancel(ignored_ec);
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    s->async_write_some(
        asio::buffer(bs->data.get(), bs->size),
        asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,buf=bs->data](
                const boost::system::error_code& ec,
                std::size_t bytes_transferred
            ) {
                boost::ignore_unused(buf);
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

#ifndef BOOST_ASIO_USE_WOLFSSL
static int tls_socket_set_server_name(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);

    auto s = reinterpret_cast<TlsSocket*>(lua_touserdata(L, 1));
    if (!s || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &tls_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    if(!SSL_set_tlsext_host_name(s->native_handle(), lua_tostring(L, 2))) {
        boost::system::error_code ec{
            static_cast<int>(ERR_get_error()), asio::error::get_ssl_category()
        };
        push(L, ec);
        return lua_error(L);
    }

    return 0;
}
#endif // !defined(BOOST_ASIO_USE_WOLFSSL)

static int tls_socket_set_verify_callback(lua_State* L)
{
    lua_settop(L, 3);
    luaL_checktype(L, 2, LUA_TSTRING);

    auto s = reinterpret_cast<TlsSocket*>(lua_touserdata(L, 1));
    if (!s || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &tls_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("host_name_verification"),
                [&]() -> int {
                    luaL_checktype(L, 3, LUA_TSTRING);
                    asio::ssl::host_name_verification o{
                        static_cast<std::string>(tostringview(L, 3))};
                    s->set_verify_callback(std::move(o), ec);
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

static int tls_socket_set_verify_depth(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TNUMBER);

    auto s = reinterpret_cast<TlsSocket*>(lua_touserdata(L, 1));
    if (!s || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &tls_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    s->set_verify_depth(lua_tointeger(L, 2), ec);
    if (ec) {
        push(L, ec);
        return lua_error(L);
    }
    return 0;
}

static int tls_socket_set_verify_mode(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);

    auto s = reinterpret_cast<TlsSocket*>(lua_touserdata(L, 1));
    if (!s || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &tls_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("none"),
                [&]() -> int {
                    s->set_verify_mode(asio::ssl::verify_none, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("peer"),
                [&]() -> int {
                    s->set_verify_mode(asio::ssl::verify_peer, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("fail_if_no_peer_cert"),
                [&]() -> int {
                    s->set_verify_mode(
                        asio::ssl::verify_fail_if_no_peer_cert, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("client_once"),
                [&]() -> int {
                    s->set_verify_mode(asio::ssl::verify_client_once, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            )
        ),
        [L](std::string_view /*key*/) -> int {
            push(L, std::errc::invalid_argument);
            return lua_error(L);
        },
        tostringview(L, 2)
    );
    return 0;
}

static int tls_socket_mt_index(lua_State* L)
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
            ),
            hana::make_pair(
                BOOST_HANA_STRING("read_some"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX, &tls_socket_read_some_key);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("write_some"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX, &tls_socket_write_some_key);
                    return 1;
                }
            ),
#ifndef BOOST_ASIO_USE_WOLFSSL
            hana::make_pair(
                BOOST_HANA_STRING("set_server_name"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, tls_socket_set_server_name);
                    return 1;
                }
            ),
#endif // !defined(BOOST_ASIO_USE_WOLFSSL)
            hana::make_pair(
                BOOST_HANA_STRING("set_verify_callback"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, tls_socket_set_verify_callback);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("set_verify_depth"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, tls_socket_set_verify_depth);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("set_verify_mode"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, tls_socket_set_verify_mode);
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

void init_tls(lua_State* L)
{
    lua_pushlightuserdata(L, &tls_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "context_flag");
        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/9);

            lua_pushliteral(L, "default_workarounds");
            lua_pushinteger(L, asio::ssl::context_base::default_workarounds);
            lua_rawset(L, -3);

            lua_pushliteral(L, "no_compression");
            lua_pushinteger(L, asio::ssl::context_base::no_compression);
            lua_rawset(L, -3);

            lua_pushliteral(L, "no_sslv2");
            lua_pushinteger(L, asio::ssl::context_base::no_sslv2);
            lua_rawset(L, -3);

            lua_pushliteral(L, "no_sslv3");
            lua_pushinteger(L, asio::ssl::context_base::no_sslv3);
            lua_rawset(L, -3);

            lua_pushliteral(L, "no_tlsv1");
            lua_pushinteger(L, asio::ssl::context_base::no_tlsv1);
            lua_rawset(L, -3);

            lua_pushliteral(L, "no_tlsv1_1");
            lua_pushinteger(L, asio::ssl::context_base::no_tlsv1_1);
            lua_rawset(L, -3);

            lua_pushliteral(L, "no_tlsv1_2");
            lua_pushinteger(L, asio::ssl::context_base::no_tlsv1_2);
            lua_rawset(L, -3);

            lua_pushliteral(L, "no_tlsv1_3");
            lua_pushinteger(L, asio::ssl::context_base::no_tlsv1_3);
            lua_rawset(L, -3);

            lua_pushliteral(L, "single_dh_use");
            lua_pushinteger(L, asio::ssl::context_base::single_dh_use);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);

        lua_pushliteral(L, "context");
        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/1);

            lua_pushliteral(L, "new");
            lua_pushcfunction(L, tls_context_new);
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

    lua_pushlightuserdata(L, &tls_context_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "tls.context");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, tls_context_mt_index);
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
        lua_pushcfunction(L, tls_socket_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<TlsSocket>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    rawgetp(L, LUA_REGISTRYINDEX, &var_args__retval1_to_error__key);

    lua_pushlightuserdata(L, &socket_client_handshake_key);
    lua_pushvalue(L, -2);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, socket_handshake<asio::ssl::stream_base::client>);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &socket_server_handshake_key);
    lua_insert(L, -2);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, socket_handshake<asio::ssl::stream_base::server>);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &tls_socket_read_some_key);
    rawgetp(L, LUA_REGISTRYINDEX,
            &var_args__retval1_to_error__fwd_retval2__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, tls_socket_read_some);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &tls_socket_write_some_key);
    rawgetp(L, LUA_REGISTRYINDEX,
            &var_args__retval1_to_error__fwd_retval2__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, tls_socket_write_some);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);
}

} // namespace emilua
