/* Copyright (c) 2020 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#include <type_traits>

#include <boost/beast/core/stream_traits.hpp>

#include <boost/hana.hpp>

#include <boost/http/algorithm/query.hpp>

#include <emilua/dispatch_table.hpp>
#include <emilua/fiber.hpp>
#include <emilua/http.hpp>
#include <emilua/tls.hpp>
#include <emilua/ip.hpp>

namespace emilua {

using boost::beast::get_lowest_layer;

enum class headers_origin
{
    request_headers,
    request_trailers,
    response_headers,
    response_trailers,
};

extern unsigned char http_op_bytecode[];
extern std::size_t http_op_bytecode_size;

char http_key;
char http_request_mt_key;
char http_response_mt_key;
char http_socket_mt_key;
char https_socket_mt_key;

static char headers_mt_key;

template<class T>
struct http_op_key
{
    static char close;
    static char read_request;
    static char write_response;
    static char write_response_continue;
    static char write_response_metadata;
    static char write;
    static char write_trailers;
    static char write_end_of_message;
    static char write_request;
    static char write_request_metadata;
    static char read_response;
    static char read_some;
};

template<class T> char http_op_key<T>::close;
template<class T> char http_op_key<T>::read_request;
template<class T> char http_op_key<T>::write_response;
template<class T> char http_op_key<T>::write_response_continue;
template<class T> char http_op_key<T>::write_response_metadata;
template<class T> char http_op_key<T>::write;
template<class T> char http_op_key<T>::write_trailers;
template<class T> char http_op_key<T>::write_end_of_message;
template<class T> char http_op_key<T>::write_request;
template<class T> char http_op_key<T>::write_request_metadata;
template<class T> char http_op_key<T>::read_response;
template<class T> char http_op_key<T>::read_some;

template<class T>
struct get_http_mt_key;

template<>
struct get_http_mt_key<asio::ip::tcp::socket>
{
    static void* get()
    {
        return &http_socket_mt_key;
    }
};

template<>
struct get_http_mt_key<TlsSocket>
{
    static void* get()
    {
        return &https_socket_mt_key;
    }
};

static int request_new(lua_State* L)
{
    auto r = reinterpret_cast<std::shared_ptr<Request>*>(
        lua_newuserdata(L, sizeof(std::shared_ptr<Request>))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &http_request_mt_key);
    setmetatable(L, -2);
    new (r) std::shared_ptr<Request>{std::make_shared<Request>()};
    return 1;
}

static int request_continue_required(lua_State* L)
{
    auto m = reinterpret_cast<std::shared_ptr<Request>*>(lua_touserdata(L, 1));
    if (!m || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &http_request_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    lua_pushboolean(L, request_continue_required(**m));
    return 1;
}

static int response_new(lua_State* L)
{
    auto r = reinterpret_cast<std::shared_ptr<Response>*>(
        lua_newuserdata(L, sizeof(std::shared_ptr<Response>))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &http_response_mt_key);
    setmetatable(L, -2);
    new (r) std::shared_ptr<Response>{std::make_shared<Response>()};
    return 1;
}

static int socket_new(lua_State* L)
{
    lua_settop(L, 1);

    if (!lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    asio::ip::tcp::socket* tcp_sock = nullptr;
    TlsSocket* tls_sock = nullptr;

    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_socket_mt_key);
    if (lua_rawequal(L, -1, -2)) {
        tcp_sock = reinterpret_cast<decltype(tcp_sock)>(lua_touserdata(L, 1));
    } else {
        rawgetp(L, LUA_REGISTRYINDEX, &tls_socket_mt_key);
        if (!lua_rawequal(L, -1, -3)) {
            push(L, std::errc::invalid_argument, "arg", 1);
            return lua_error(L);
        }
        tls_sock = reinterpret_cast<decltype(tls_sock)>(lua_touserdata(L, 1));
    }
    assert(tcp_sock || tls_sock);

    auto make = [&](auto& s1) {
        using T = std::decay_t<decltype(s1)>;
        auto s = reinterpret_cast<Socket<T>*>(lua_newuserdata(
            L, sizeof(Socket<T>)
        ));
        rawgetp(L, LUA_REGISTRYINDEX, get_http_mt_key<T>::get());
        setmetatable(L, -2);
        new (s) Socket<T>{std::move(s1)};

        lua_pushnil(L);
        setmetatable(L, 1);
        s1.~T();
    };
    if (tcp_sock) make(*tcp_sock);
    else make(*tls_sock);

    return 1;
}

inline int request_method(lua_State* L)
{
    auto& r = *reinterpret_cast<std::shared_ptr<Request>*>(
        lua_touserdata(L, 1));
    push(L, r->method());
    return 1;
}

inline int request_target(lua_State* L)
{
    auto& r = *reinterpret_cast<std::shared_ptr<Request>*>(
        lua_touserdata(L, 1));
    push(L, r->target());
    return 1;
}

template<headers_origin ORIGIN>
inline int message_headers(lua_State* L)
{
    *reinterpret_cast<headers_origin*>(
        lua_newuserdata(L, sizeof(headers_origin))
    ) = ORIGIN;

    rawgetp(L, LUA_REGISTRYINDEX, &headers_mt_key);
    setmetatable(L, -2);

    lua_createtable(L, /*narr=*/1, /*nrec=*/0);
    lua_pushvalue(L, 1);
    lua_rawseti(L, -2, 1);
    lua_setfenv(L, -2);

    return 1;
}

template<class T>
inline int message_body(lua_State* L)
{
    auto& r = *reinterpret_cast<std::shared_ptr<T>*>(lua_touserdata(L, 1));
    lua_pushlstring(L, reinterpret_cast<char*>(r->body().data()),
                    r->body().size());
    return 1;
}

static int request_meta_index(lua_State* L)
{
    auto& r = *reinterpret_cast<std::shared_ptr<Request>*>(
        lua_touserdata(L, 1));
    if (r->has_writer) {
        push(L, std::errc::device_or_resource_busy);
        return lua_error(L);
    }

    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(BOOST_HANA_STRING("method"), request_method),
            hana::make_pair(BOOST_HANA_STRING("target"), request_target),
            hana::make_pair(
                BOOST_HANA_STRING("headers"),
                message_headers<headers_origin::request_headers>),
            hana::make_pair(BOOST_HANA_STRING("body"), message_body<Request>),
            hana::make_pair(
                BOOST_HANA_STRING("trailers"),
                message_headers<headers_origin::request_trailers>)
        ),
        [](std::string_view /*key*/, lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        },
        tostringview(L, 2),
        L
    );
}

inline int request_newmethod(lua_State* L)
{
    luaL_checktype(L, 3, LUA_TSTRING);
    auto& r = *reinterpret_cast<std::shared_ptr<Request>*>(
        lua_touserdata(L, 1));
    r->method() = tostringview(L, 3);
    return 0;
}

inline int request_newtarget(lua_State* L)
{
    luaL_checktype(L, 3, LUA_TSTRING);
    auto& r = *reinterpret_cast<std::shared_ptr<Request>*>(
        lua_touserdata(L, 1));
    r->target() = tostringview(L, 3);
    return 0;
}

template<headers_origin ORIGIN>
inline int message_newheaders(lua_State* L)
{
    static constexpr auto headers_getter = hana::make_map(
        hana::make_pair(
            hana::int_c<static_cast<int>(headers_origin::request_headers)>,
            [](lua_State* L) -> Headers& {
                auto& r = *reinterpret_cast<std::shared_ptr<Request>*>(
                    lua_touserdata(L, 1));
                return r->headers();
            }),
        hana::make_pair(
            hana::int_c<static_cast<int>(headers_origin::request_trailers)>,
            [](lua_State* L) -> Headers& {
                auto& r = *reinterpret_cast<std::shared_ptr<Request>*>(
                    lua_touserdata(L, 1));
                return r->trailers();
            }),
        hana::make_pair(
            hana::int_c<static_cast<int>(headers_origin::response_headers)>,
            [](lua_State* L) -> Headers& {
                auto& r = *reinterpret_cast<std::shared_ptr<Response>*>(
                    lua_touserdata(L, 1));
                return r->headers();
            }),
        hana::make_pair(
            hana::int_c<static_cast<int>(headers_origin::response_trailers)>,
            [](lua_State* L) -> Headers& {
                auto& r = *reinterpret_cast<std::shared_ptr<Response>*>(
                    lua_touserdata(L, 1));
                return r->trailers();
            })
    );

    Headers& headers = headers_getter[hana::int_c<static_cast<int>(ORIGIN)>](L);
    switch (lua_type(L, 3)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    case LUA_TNIL:
        headers.clear();
        break;
    case LUA_TTABLE:
        if (lua_getmetatable(L, 3)) {
            push(L, std::errc::invalid_argument, "arg", 3);
            return lua_error(L);
        }

        headers.clear();

        lua_pushnil(L);
        while (lua_next(L, 3) != 0) {
            if (lua_type(L, -2) != LUA_TSTRING) {
                lua_pop(L, 1);
                continue;
            }

            auto key = tostringview(L, -2);
            switch (lua_type(L, -1)) {
            case LUA_TSTRING:
                headers.emplace(key, tostringview(L, -1));
                break;
            case LUA_TTABLE: {
                if (lua_getmetatable(L, -1)) {
                    lua_pop(L, 1);
                    break;
                }

                Headers values;

                for (int i = 1 ;; ++i) {
                    lua_rawgeti(L, -1, i);
                    if (lua_type(L, -1) != LUA_TSTRING) {
                        lua_pop(L, 1);
                        break;
                    }

                    values.emplace_hint(values.end(), key, tostringview(L, -1));
                    lua_pop(L, 1);
                }

                headers.merge(std::move(values));
                break;
            }
            default: break;
            }
            lua_pop(L, 1);
        }
        break;
    }
    return 0;
}

template<class T>
inline int message_newbody(lua_State* L)
{
    switch (lua_type(L, 3)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    case LUA_TSTRING: {
        luaL_checktype(L, 3, LUA_TSTRING);
        auto& r = *reinterpret_cast<std::shared_ptr<T>*>(lua_touserdata(L, 1));
        auto b = tostringview(L, 3);
        r->body().resize(b.size());
        std::memcpy(r->body().data(), b.data(), b.size());
        return 0;
    }
    case LUA_TNIL: {
        auto& r = *reinterpret_cast<std::shared_ptr<T>*>(lua_touserdata(L, 1));
        r->body().clear();
        return 0;
    }
    }
}

static int request_meta_newindex(lua_State* L)
{
    auto& r = *reinterpret_cast<std::shared_ptr<Request>*>(
        lua_touserdata(L, 1));
    if (r->nreaders > 0 || r->has_writer) {
        push(L, std::errc::device_or_resource_busy);
        return lua_error(L);
    }

    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(BOOST_HANA_STRING("method"), request_newmethod),
            hana::make_pair(BOOST_HANA_STRING("target"), request_newtarget),
            hana::make_pair(
                BOOST_HANA_STRING("headers"),
                message_newheaders<headers_origin::request_headers>),
            hana::make_pair(
                BOOST_HANA_STRING("body"), message_newbody<Request>),
            hana::make_pair(
                BOOST_HANA_STRING("trailers"),
                message_newheaders<headers_origin::request_trailers>)
        ),
        [](std::string_view /*key*/, lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        },
        tostringview(L, 2),
        L
    );
}

inline int response_status(lua_State* L)
{
    auto& r = *reinterpret_cast<std::shared_ptr<Response>*>(
        lua_touserdata(L, 1));
    lua_pushinteger(L, r->status_code());
    return 1;
}

inline int response_reason(lua_State* L)
{
    auto& r = *reinterpret_cast<std::shared_ptr<Response>*>(
        lua_touserdata(L, 1));
    push(L, r->reason_phrase());
    return 1;
}

static int response_meta_index(lua_State* L)
{
    auto& r = *reinterpret_cast<std::shared_ptr<Response>*>(
        lua_touserdata(L, 1));
    if (r->has_writer) {
        push(L, std::errc::device_or_resource_busy);
        return lua_error(L);
    }

    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(BOOST_HANA_STRING("status"), response_status),
            hana::make_pair(BOOST_HANA_STRING("reason"), response_reason),
            hana::make_pair(
                BOOST_HANA_STRING("headers"),
                message_headers<headers_origin::response_headers>),
            hana::make_pair(BOOST_HANA_STRING("body"), message_body<Response>),
            hana::make_pair(
                BOOST_HANA_STRING("trailers"),
                message_headers<headers_origin::response_trailers>)
        ),
        [](std::string_view /*key*/, lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        },
        tostringview(L, 2),
        L
    );
}

inline int response_newstatus(lua_State* L)
{
    luaL_checktype(L, 3, LUA_TNUMBER);
    auto& r = *reinterpret_cast<std::shared_ptr<Response>*>(
        lua_touserdata(L, 1));
    r->status_code() = lua_tointeger(L, 3);
    return 0;
}

inline int response_newreason(lua_State* L)
{
    luaL_checktype(L, 3, LUA_TSTRING);
    auto& r = *reinterpret_cast<std::shared_ptr<Response>*>(
        lua_touserdata(L, 1));
    r->reason_phrase() = tostringview(L, 3);
    return 0;
}

static int response_meta_newindex(lua_State* L)
{
    auto& r = *reinterpret_cast<std::shared_ptr<Response>*>(
        lua_touserdata(L, 1));
    if (r->nreaders > 0 || r->has_writer) {
        push(L, std::errc::device_or_resource_busy);
        return lua_error(L);
    }

    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(BOOST_HANA_STRING("status"), response_newstatus),
            hana::make_pair(BOOST_HANA_STRING("reason"), response_newreason),
            hana::make_pair(
                BOOST_HANA_STRING("headers"),
                message_newheaders<headers_origin::response_headers>),
            hana::make_pair(
                BOOST_HANA_STRING("body"), message_newbody<Response>),
            hana::make_pair(
                BOOST_HANA_STRING("trailers"),
                message_newheaders<headers_origin::response_trailers>)
        ),
        [](std::string_view /*key*/, lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        },
        tostringview(L, 2),
        L
    );
}

static int headers_meta_index(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);
    auto key = tostringview(L, 2);

    lua_getfenv(L, 1);
    lua_rawgeti(L, -1, 1);

    Headers* headers;
    switch (*reinterpret_cast<headers_origin*>(lua_touserdata(L, 1))) {
    case headers_origin::request_headers: {
        auto& r = *reinterpret_cast<std::shared_ptr<Request>*>(
            lua_touserdata(L, -1));
        if (r->has_writer) {
            push(L, std::errc::device_or_resource_busy);
            return lua_error(L);
        }

        headers = &r->headers();
        break;
    }
    case headers_origin::request_trailers: {
        auto& r = *reinterpret_cast<std::shared_ptr<Request>*>(
            lua_touserdata(L, -1));
        if (r->has_writer) {
            push(L, std::errc::device_or_resource_busy);
            return lua_error(L);
        }

        headers = &r->trailers();
        break;
    }
    case headers_origin::response_headers: {
        auto& r = *reinterpret_cast<std::shared_ptr<Response>*>(
            lua_touserdata(L, -1));
        if (r->has_writer) {
            push(L, std::errc::device_or_resource_busy);
            return lua_error(L);
        }

        headers = &r->headers();
        break;
    }
    case headers_origin::response_trailers: {
        auto& r = *reinterpret_cast<std::shared_ptr<Response>*>(
            lua_touserdata(L, -1));
        if (r->has_writer) {
            push(L, std::errc::device_or_resource_busy);
            return lua_error(L);
        }

        headers = &r->trailers();
        break;
    }
    default:
        assert(false);
    }

    auto range = headers->equal_range(key);

    if (range.first == range.second)
        return 0;

    if (auto it = range.first; ++it == range.second) {
        push(L, range.first->second);
        return 1;
    }

    lua_newtable(L);
    for (int i = 1 ; range.first != range.second ; ++i, ++range.first) {
        push(L, range.first->second);
        lua_rawseti(L, -2, i);
    }
    return 1;
}

static int headers_meta_newindex(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);
    auto key = tostringview(L, 2);

    lua_getfenv(L, 1);
    lua_rawgeti(L, -1, 1);

    Headers* headers;
    switch (*reinterpret_cast<headers_origin*>(lua_touserdata(L, 1))) {
    case headers_origin::request_headers: {
        auto& r = *reinterpret_cast<std::shared_ptr<Request>*>(
            lua_touserdata(L, -1));
        if (r->nreaders > 0 || r->has_writer) {
            push(L, std::errc::device_or_resource_busy);
            return lua_error(L);
        }

        headers = &r->headers();
        break;
    }
    case headers_origin::request_trailers: {
        auto& r = *reinterpret_cast<std::shared_ptr<Request>*>(
            lua_touserdata(L, -1));
        if (r->nreaders > 0 || r->has_writer) {
            push(L, std::errc::device_or_resource_busy);
            return lua_error(L);
        }

        headers = &r->trailers();
        break;
    }
    case headers_origin::response_headers: {
        auto& r = *reinterpret_cast<std::shared_ptr<Response>*>(
            lua_touserdata(L, -1));
        if (r->nreaders > 0 || r->has_writer) {
            push(L, std::errc::device_or_resource_busy);
            return lua_error(L);
        }

        headers = &r->headers();
        break;
    }
    case headers_origin::response_trailers: {
        auto& r = *reinterpret_cast<std::shared_ptr<Response>*>(
            lua_touserdata(L, -1));
        if (r->nreaders > 0 || r->has_writer) {
            push(L, std::errc::device_or_resource_busy);
            return lua_error(L);
        }

        headers = &r->trailers();
        break;
    }
    default:
        assert(false);
    }

    switch (lua_type(L, 3)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    case LUA_TNIL: {
        auto range = headers->equal_range(key);
        headers->erase(range.first, range.second);
        break;
    }
    case LUA_TSTRING: {
        auto value = tostringview(L, 3);
        auto range = headers->equal_range(key);

        if (range.first == range.second) {
            headers->emplace(key, value);
        } else {
            auto hint = headers->erase(range.first, range.second);
            headers->emplace_hint(hint, key, value);
        }
        break;
    }
    case LUA_TTABLE: {
        if (lua_getmetatable(L, 3)) {
            push(L, std::errc::invalid_argument, "arg", 3);
            return lua_error(L);
        }

        Headers new_headers;

        for (int i = 1 ;; ++i) {
            lua_rawgeti(L, 3, i);
            if (lua_type(L, -1) != LUA_TSTRING)
                break;

            auto value = tostringview(L, -1);
            new_headers.emplace_hint(new_headers.end(), key, value);
            lua_pop(L, 1);
        }

        if (auto range = headers->equal_range(key) ;
            range.first != range.second) {
            headers->erase(range.first, range.second);
        }
        headers->merge(std::move(new_headers));
        break;
    }
    }
    return 0;
}

static int headers_next(lua_State* L)
{
    lua_settop(L, 2);
    auto origin = reinterpret_cast<headers_origin*>(lua_touserdata(L, 1));
    if (!origin || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &headers_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    lua_getfenv(L, 1);
    lua_rawgeti(L, -1, 1);

    Headers* headers;
    switch (*origin) {
    case headers_origin::request_headers: {
        auto& r = *reinterpret_cast<std::shared_ptr<Request>*>(
            lua_touserdata(L, -1));
        headers = &r->headers();
        break;
    }
    case headers_origin::request_trailers: {
        auto& r = *reinterpret_cast<std::shared_ptr<Request>*>(
            lua_touserdata(L, -1));
        headers = &r->trailers();
        break;
    }
    case headers_origin::response_headers: {
        auto& r = *reinterpret_cast<std::shared_ptr<Response>*>(
            lua_touserdata(L, -1));
        headers = &r->headers();
        break;
    }
    case headers_origin::response_trailers: {
        auto& r = *reinterpret_cast<std::shared_ptr<Response>*>(
            lua_touserdata(L, -1));
        headers = &r->trailers();
        break;
    }
    default:
        assert(false);
    }

    Headers::iterator begin;

    switch (lua_type(L, 2)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    case LUA_TNIL:
        begin = headers->begin();
        break;
    case LUA_TSTRING:
        begin = headers->upper_bound(tostringview(L, 2));
        break;
    }

    if (begin == headers->end())
        return 0;

    auto end = headers->upper_bound(begin->first);
    push(L, begin->first);

    if (auto it = begin ; ++it == end) {
        push(L, begin->second);
        return 2;
    }

    lua_newtable(L);
    for (int i = 1 ; begin != end ; ++i, ++begin) {
        push(L, begin->second);
        lua_rawseti(L, -2, i);
    }

    return 2;
}

static int headers_meta_pairs(lua_State* L)
{
    lua_pushcfunction(L, headers_next);
    lua_pushvalue(L, 1);
    return 2;
}

template<class T>
static int socket_close(lua_State* L);

template<>
int socket_close<asio::ip::tcp::socket>(lua_State* L)
{
    auto s = reinterpret_cast<Socket<asio::ip::tcp::socket>*>(
        lua_touserdata(L, 1));
    if (!s || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &http_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    s->socket.next_layer().close(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }

    return 0;
}

template<>
int socket_close<TlsSocket>(lua_State* L)
{
    lua_settop(L, 1);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto s = reinterpret_cast<Socket<TlsSocket>*>(lua_touserdata(L, 1));
    if (!s || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &https_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    lua_pushvalue(L, 1);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto s = reinterpret_cast<Socket<TlsSocket>*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            boost::system::error_code ignored_ec;
            get_lowest_layer(s->socket).close(ignored_ec);
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    ++s->nbusy;
    s->socket.next_layer().async_shutdown(asio::bind_executor(
        vm_ctx->strand_using_defer(),
        [vm_ctx,current_fiber,s](const boost::system::error_code& ec) {
            std::error_code std_ec = ec;
            vm_ctx->fiber_prologue(
                current_fiber,
                [&]() {
                    boost::system::error_code ec2;
                    s->socket.next_layer().next_layer().close(ec2);

                    --s->nbusy;
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

                    if (!std_ec)
                        std_ec = ec2;

                    push(current_fiber, std_ec);
                });
            int res = lua_resume(current_fiber, 1);
            vm_ctx->fiber_epilogue(res);
        }
    ));

    return lua_yield(L, 0);
}

template<class T>
static int socket_lock_client_to_http10(lua_State* L)
{
    auto s = reinterpret_cast<Socket<T>*>(lua_touserdata(L, 1));
    if (!s || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, get_http_mt_key<T>::get());
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    s->socket.lock_client_to_http10();
    return 0;
}

template<class T>
static int socket_read_request(lua_State* L)
{
    lua_settop(L, 2);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto s = reinterpret_cast<Socket<T>*>(lua_touserdata(L, 1));
    if (!s || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, get_http_mt_key<T>::get());
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto m = reinterpret_cast<std::shared_ptr<Request>*>(lua_touserdata(L, 2));
    if (!m || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &http_request_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    if ((*m)->nreaders > 0 || (*m)->has_writer) {
        push(L, std::errc::device_or_resource_busy, "arg", 2);
        return lua_error(L);
    }

    lua_pushvalue(L, 1);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto s = reinterpret_cast<Socket<T>*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            boost::system::error_code ignored_ec;
            get_lowest_layer(s->socket).close(ignored_ec);
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    ++s->nbusy;
    (*m)->has_writer = true;
    s->socket.async_read_request(**m, asio::bind_executor(
        vm_ctx->strand_using_defer(),
        [vm_ctx,current_fiber,b1=s->buffer,msg=*m,nbusy=&s->nbusy](
            const boost::system::error_code& ec
        ) {
            boost::ignore_unused(b1);
            msg->has_writer = false;
            std::error_code std_ec = ec;
            vm_ctx->fiber_prologue(
                current_fiber,
                [&]() {
                    --*nbusy;
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

template<class T>
static int socket_write_response(lua_State* L)
{
    lua_settop(L, 2);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto s = reinterpret_cast<Socket<T>*>(lua_touserdata(L, 1));
    if (!s || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, get_http_mt_key<T>::get());
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto m = reinterpret_cast<std::shared_ptr<Response>*>(lua_touserdata(L, 2));
    if (!m || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &http_response_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    if ((*m)->has_writer || (*m)->nreaders == (*m)->max_readers) {
        push(L, std::errc::device_or_resource_busy, "arg", 2);
        return lua_error(L);
    }

    lua_pushvalue(L, 1);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto s = reinterpret_cast<Socket<T>*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            boost::system::error_code ignored_ec;
            get_lowest_layer(s->socket).close(ignored_ec);
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    ++s->nbusy;
    (*m)->nreaders++;
    s->socket.async_write_response(**m, asio::bind_executor(
        vm_ctx->strand_using_defer(),
        [vm_ctx,current_fiber,msg=*m,nbusy=&s->nbusy](
            const boost::system::error_code& ec
        ) {
            msg->nreaders--;
            std::error_code std_ec = ec;
            vm_ctx->fiber_prologue(
                current_fiber,
                [&]() {
                    --*nbusy;
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

template<class T>
static int socket_write_response_continue(lua_State* L)
{
    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto s = reinterpret_cast<Socket<T>*>(lua_touserdata(L, 1));
    if (!s || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, get_http_mt_key<T>::get());
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    lua_pushvalue(L, 1);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto s = reinterpret_cast<Socket<T>*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            boost::system::error_code ignored_ec;
            get_lowest_layer(s->socket).close(ignored_ec);
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    ++s->nbusy;
    s->socket.async_write_response_continue(asio::bind_executor(
        vm_ctx->strand_using_defer(),
        [vm_ctx,current_fiber,nbusy=&s->nbusy](
            const boost::system::error_code& ec
        ) {
            std::error_code std_ec = ec;
            vm_ctx->fiber_prologue(
                current_fiber,
                [&]() {
                    --*nbusy;
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

template<class T>
static int socket_write_response_metadata(lua_State* L)
{
    lua_settop(L, 2);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto s = reinterpret_cast<Socket<T>*>(lua_touserdata(L, 1));
    if (!s || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, get_http_mt_key<T>::get());
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto m = reinterpret_cast<std::shared_ptr<Response>*>(lua_touserdata(L, 2));
    if (!m || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &http_response_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    if ((*m)->has_writer || (*m)->nreaders == (*m)->max_readers) {
        push(L, std::errc::device_or_resource_busy, "arg", 2);
        return lua_error(L);
    }

    lua_pushvalue(L, 1);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto s = reinterpret_cast<Socket<T>*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            boost::system::error_code ignored_ec;
            get_lowest_layer(s->socket).close(ignored_ec);
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    ++s->nbusy;
    (*m)->nreaders++;
    s->socket.async_write_response_metadata(**m, asio::bind_executor(
        vm_ctx->strand_using_defer(),
        [vm_ctx,current_fiber,msg=*m,nbusy=&s->nbusy](
            const boost::system::error_code& ec
        ) {
            msg->nreaders--;
            std::error_code std_ec = ec;
            vm_ctx->fiber_prologue(
                current_fiber,
                [&]() {
                    --*nbusy;
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

template<class T>
static int socket_write(lua_State* L)
{
    lua_settop(L, 2);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto s = reinterpret_cast<Socket<T>*>(lua_touserdata(L, 1));
    if (!s || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, get_http_mt_key<T>::get());
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    if (!lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    std::shared_ptr<Request>* req = nullptr;
    std::shared_ptr<Response>* res = nullptr;

    rawgetp(L, LUA_REGISTRYINDEX, &http_request_mt_key);
    if (lua_rawequal(L, -1, -2)) {
        req = reinterpret_cast<decltype(req)>(lua_touserdata(L, 2));
    } else {
        rawgetp(L, LUA_REGISTRYINDEX, &http_response_mt_key);
        if (!lua_rawequal(L, -1, -3)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        res = reinterpret_cast<decltype(res)>(lua_touserdata(L, 2));
    }
    assert(req || res);

    if ((req && ((*req)->has_writer ||
                 (*req)->nreaders == (*req)->max_readers)) ||
        (res && ((*res)->has_writer ||
                 (*res)->nreaders == (*res)->max_readers))) {
        push(L, std::errc::device_or_resource_busy, "arg", 2);
        return lua_error(L);
    }

    lua_pushvalue(L, 1);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto s = reinterpret_cast<Socket<T>*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            boost::system::error_code ignored_ec;
            get_lowest_layer(s->socket).close(ignored_ec);
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    auto sched_op = [&](auto& msg) {
        ++s->nbusy;
        msg->nreaders++;
        s->socket.async_write(*msg, asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,msg,nbusy=&s->nbusy](
                const boost::system::error_code& ec
            ) {
                msg->nreaders--;
                std::error_code std_ec = ec;
                vm_ctx->fiber_prologue(
                    current_fiber,
                    [&]() {
                        --*nbusy;
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
    };
    if (req) sched_op(*req);
    else sched_op(*res);

    return lua_yield(L, 0);
}

template<class T>
static int socket_write_trailers(lua_State* L)
{
    lua_settop(L, 2);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto s = reinterpret_cast<Socket<T>*>(lua_touserdata(L, 1));
    if (!s || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, get_http_mt_key<T>::get());
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    if (!lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    std::shared_ptr<Request>* req = nullptr;
    std::shared_ptr<Response>* res = nullptr;

    rawgetp(L, LUA_REGISTRYINDEX, &http_request_mt_key);
    if (lua_rawequal(L, -1, -2)) {
        req = reinterpret_cast<decltype(req)>(lua_touserdata(L, 2));
    } else {
        rawgetp(L, LUA_REGISTRYINDEX, &http_response_mt_key);
        if (!lua_rawequal(L, -1, -3)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        res = reinterpret_cast<decltype(res)>(lua_touserdata(L, 2));
    }
    assert(req || res);

    if ((req && ((*req)->has_writer ||
                 (*req)->nreaders == (*req)->max_readers)) ||
        (res && ((*res)->has_writer ||
                 (*res)->nreaders == (*res)->max_readers))) {
        push(L, std::errc::device_or_resource_busy, "arg", 2);
        return lua_error(L);
    }

    lua_pushvalue(L, 1);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto s = reinterpret_cast<Socket<T>*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            boost::system::error_code ignored_ec;
            get_lowest_layer(s->socket).close(ignored_ec);
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    auto sched_op = [&](auto& msg) {
        ++s->nbusy;
        msg->nreaders++;
        s->socket.async_write_trailers(*msg, asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,msg,nbusy=&s->nbusy](
                const boost::system::error_code& ec
            ) {
                msg->nreaders--;
                std::error_code std_ec = ec;
                vm_ctx->fiber_prologue(
                    current_fiber,
                    [&]() {
                        --*nbusy;
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
    };
    if (req) sched_op(*req);
    else sched_op(*res);

    return lua_yield(L, 0);
}

template<class T>
static int socket_write_end_of_message(lua_State* L)
{
    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto s = reinterpret_cast<Socket<T>*>(lua_touserdata(L, 1));
    if (!s || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, get_http_mt_key<T>::get());
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    lua_pushvalue(L, 1);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto s = reinterpret_cast<Socket<T>*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            boost::system::error_code ignored_ec;
            get_lowest_layer(s->socket).close(ignored_ec);
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    ++s->nbusy;
    s->socket.async_write_end_of_message(asio::bind_executor(
        vm_ctx->strand_using_defer(),
        [vm_ctx,current_fiber,nbusy=&s->nbusy](
            const boost::system::error_code& ec
        ) {
            std::error_code std_ec = ec;
            vm_ctx->fiber_prologue(
                current_fiber,
                [&]() {
                    --*nbusy;
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

template<class T>
static int socket_write_request(lua_State* L)
{
    lua_settop(L, 2);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto s = reinterpret_cast<Socket<T>*>(lua_touserdata(L, 1));
    if (!s || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, get_http_mt_key<T>::get());
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto m = reinterpret_cast<std::shared_ptr<Request>*>(lua_touserdata(L, 2));
    if (!m || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &http_request_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    if ((*m)->has_writer || (*m)->nreaders == (*m)->max_readers) {
        push(L, std::errc::device_or_resource_busy, "arg", 2);
        return lua_error(L);
    }

    lua_pushvalue(L, 1);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto s = reinterpret_cast<Socket<T>*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            boost::system::error_code ignored_ec;
            get_lowest_layer(s->socket).close(ignored_ec);
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    ++s->nbusy;
    (*m)->nreaders++;
    s->socket.async_write_request(**m, asio::bind_executor(
        vm_ctx->strand_using_defer(),
        [vm_ctx,current_fiber,msg=*m,nbusy=&s->nbusy](
            const boost::system::error_code& ec
        ) {
            msg->nreaders--;
            std::error_code std_ec = ec;
            vm_ctx->fiber_prologue(
                current_fiber,
                [&]() {
                    --*nbusy;
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

template<class T>
static int socket_write_request_metadata(lua_State* L)
{
    lua_settop(L, 2);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto s = reinterpret_cast<Socket<T>*>(lua_touserdata(L, 1));
    if (!s || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, get_http_mt_key<T>::get());
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto m = reinterpret_cast<std::shared_ptr<Request>*>(lua_touserdata(L, 2));
    if (!m || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &http_request_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    if ((*m)->has_writer || (*m)->nreaders == (*m)->max_readers) {
        push(L, std::errc::device_or_resource_busy, "arg", 2);
        return lua_error(L);
    }

    lua_pushvalue(L, 1);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto s = reinterpret_cast<Socket<T>*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            boost::system::error_code ignored_ec;
            get_lowest_layer(s->socket).close(ignored_ec);
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    ++s->nbusy;
    (*m)->nreaders++;
    s->socket.async_write_request_metadata(**m, asio::bind_executor(
        vm_ctx->strand_using_defer(),
        [vm_ctx,current_fiber,msg=*m,nbusy=&s->nbusy](
            const boost::system::error_code& ec
        ) {
            msg->nreaders--;
            std::error_code std_ec = ec;
            vm_ctx->fiber_prologue(
                current_fiber,
                [&]() {
                    --*nbusy;
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

template<class T>
static int socket_read_response(lua_State* L)
{
    lua_settop(L, 2);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto s = reinterpret_cast<Socket<T>*>(lua_touserdata(L, 1));
    if (!s || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, get_http_mt_key<T>::get());
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto m = reinterpret_cast<std::shared_ptr<Response>*>(lua_touserdata(L, 2));
    if (!m || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &http_response_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    if ((*m)->nreaders > 0 || (*m)->has_writer) {
        push(L, std::errc::device_or_resource_busy, "arg", 2);
        return lua_error(L);
    }

    lua_pushvalue(L, 1);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto s = reinterpret_cast<Socket<T>*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            boost::system::error_code ignored_ec;
            get_lowest_layer(s->socket).close(ignored_ec);
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    ++s->nbusy;
    (*m)->has_writer = true;
    s->socket.async_read_response(**m, asio::bind_executor(
        vm_ctx->strand_using_defer(),
        [vm_ctx,current_fiber,b1=s->buffer,msg=*m,nbusy=&s->nbusy](
            const boost::system::error_code& ec
        ) {
            boost::ignore_unused(b1);
            msg->has_writer = false;
            std::error_code std_ec = ec;
            vm_ctx->fiber_prologue(
                current_fiber,
                [&]() {
                    --*nbusy;
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

template<class T>
static int socket_read_some(lua_State* L)
{
    lua_settop(L, 2);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto s = reinterpret_cast<Socket<T>*>(lua_touserdata(L, 1));
    if (!s || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, get_http_mt_key<T>::get());
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    if (!lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    std::shared_ptr<Request>* req = nullptr;
    std::shared_ptr<Response>* res = nullptr;

    rawgetp(L, LUA_REGISTRYINDEX, &http_request_mt_key);
    if (lua_rawequal(L, -1, -2)) {
        req = reinterpret_cast<decltype(req)>(lua_touserdata(L, 2));
    } else {
        rawgetp(L, LUA_REGISTRYINDEX, &http_response_mt_key);
        if (!lua_rawequal(L, -1, -3)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        res = reinterpret_cast<decltype(res)>(lua_touserdata(L, 2));
    }
    assert(req || res);

    if ((req && ((*req)->nreaders > 0 || (*req)->has_writer)) ||
        (res && ((*res)->nreaders > 0 || (*res)->has_writer))) {
        push(L, std::errc::device_or_resource_busy, "arg", 2);
        return lua_error(L);
    }

    lua_pushvalue(L, 1);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto s = reinterpret_cast<Socket<T>*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            boost::system::error_code ignored_ec;
            get_lowest_layer(s->socket).close(ignored_ec);
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    auto sched_op = [&](auto& msg) {
        ++s->nbusy;
        msg->has_writer = true;
        s->socket.async_read_some(*msg, asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,b1=s->buffer,msg,nbusy=&s->nbusy](
                const boost::system::error_code& ec
            ) {
                boost::ignore_unused(b1);
                msg->has_writer = false;
                std::error_code std_ec = ec;
                vm_ctx->fiber_prologue(
                    current_fiber,
                    [&]() {
                        --*nbusy;
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
    };
    if (req) sched_op(*req);
    else sched_op(*res);

    return lua_yield(L, 0);
}

template<class T>
inline int socket_is_open(lua_State* L)
{
    auto s = reinterpret_cast<Socket<T>*>(lua_touserdata(L, 1));
    lua_pushboolean(L, s->socket.is_open() ? 1 : 0);
    return 1;
}

template<class T>
inline int socket_read_state(lua_State* L)
{
    auto s = reinterpret_cast<Socket<T>*>(lua_touserdata(L, 1));
    std::string_view ret;
    switch (s->socket.read_state()) {
    case http::read_state::empty:
        ret = "empty";
        break;
    case http::read_state::message_ready:
        ret = "message_ready";
        break;
    case http::read_state::body_ready:
        ret = "body_ready";
        break;
    case http::read_state::finished:
        ret = "finished";
        break;
    }
    push(L, ret);
    return 1;
}

template<class T>
inline int socket_write_state(lua_State* L)
{
    auto s = reinterpret_cast<Socket<T>*>(lua_touserdata(L, 1));
    std::string_view ret;
    switch (s->socket.write_state()) {
    case http::write_state::empty:
        ret = "empty";
        break;
    case http::write_state::continue_issued:
        ret = "continue_issued";
        break;
    case http::write_state::metadata_issued:
        ret = "metadata_issued";
        break;
    case http::write_state::finished:
        ret = "finished";
        break;
    }
    push(L, ret);
    return 1;
}

template<class T>
inline int socket_is_write_response_native_stream(lua_State* L)
{
    auto s = reinterpret_cast<Socket<T>*>(lua_touserdata(L, 1));
    if (s->socket.read_state() == http::read_state::empty) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    lua_pushboolean(L, s->socket.write_response_native_stream() ? 1 : 0);
    return 1;
}

template<class T>
static int socket_meta_index(lua_State* L)
{
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("close"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX, &http_op_key<T>::close);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("lock_client_to_http10"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, socket_lock_client_to_http10<T>);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("read_request"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX,
                            &http_op_key<T>::read_request);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("write_response"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX,
                            &http_op_key<T>::write_response);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("write_response_continue"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX,
                            &http_op_key<T>::write_response_continue);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("write_response_metadata"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX,
                            &http_op_key<T>::write_response_metadata);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("write"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX, &http_op_key<T>::write);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("write_trailers"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX,
                            &http_op_key<T>::write_trailers);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("write_end_of_message"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX,
                            &http_op_key<T>::write_end_of_message);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("write_request"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX,
                            &http_op_key<T>::write_request);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("write_request_metadata"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX,
                            &http_op_key<T>::write_request_metadata);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("read_response"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX,
                            &http_op_key<T>::read_response);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("read_some"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX, &http_op_key<T>::read_some);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("is_open"), socket_is_open<T>),
            hana::make_pair(
                BOOST_HANA_STRING("read_state"), socket_read_state<T>),
            hana::make_pair(
                BOOST_HANA_STRING("write_state"), socket_write_state<T>),
            hana::make_pair(
                BOOST_HANA_STRING("is_write_response_native_stream"),
                socket_is_write_response_native_stream<T>)
        ),
        [](std::string_view /*key*/, lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        },
        tostringview(L, 2),
        L
    );
}

void init_http(lua_State* L)
{
    lua_pushlightuserdata(L, &http_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "request");
        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/2);

            lua_pushliteral(L, "new");
            lua_pushcfunction(L, request_new);
            lua_rawset(L, -3);

            lua_pushliteral(L, "continue_required");
            lua_pushcfunction(L, request_continue_required);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);

        lua_pushliteral(L, "response");
        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/1);

            lua_pushliteral(L, "new");
            lua_pushcfunction(L, response_new);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);

        lua_pushliteral(L, "socket");
        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/1);

            lua_pushliteral(L, "new");
            lua_pushcfunction(L, socket_new);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &http_request_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/4);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "http.request");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, request_meta_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__newindex");
        lua_pushcfunction(L, request_meta_newindex);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<std::shared_ptr<Request>>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &http_response_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/4);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "http.response");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, response_meta_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__newindex");
        lua_pushcfunction(L, response_meta_newindex);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<std::shared_ptr<Response>>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &headers_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/4);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "http.headers");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, headers_meta_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__newindex");
        lua_pushcfunction(L, headers_meta_newindex);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__pairs");
        lua_pushcfunction(L, headers_meta_pairs);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    int res = luaL_loadbuffer(L, reinterpret_cast<char*>(http_op_bytecode),
                              http_op_bytecode_size, nullptr);
    assert(res == 0); boost::ignore_unused(res);

    static constexpr auto register_socket = [](lua_State* L, auto type) {
        using T = typename decltype(type)::type;

        lua_pushlightuserdata(L, get_http_mt_key<T>::get());
        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/3);

            lua_pushliteral(L, "__metatable");
            lua_pushliteral(L, "http.socket");
            lua_rawset(L, -3);

            lua_pushliteral(L, "__index");
            lua_pushcfunction(L, socket_meta_index<T>);
            lua_rawset(L, -3);

            lua_pushliteral(L, "__gc");
            lua_pushcfunction(L, finalizer<Socket<T>>);
            lua_rawset(L, -3);
        }
        lua_rawset(L, LUA_REGISTRYINDEX);

        lua_pushlightuserdata(L, &http_op_key<T>::read_request);
        lua_pushvalue(L, -2);
        rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
        lua_pushcfunction(L, socket_read_request<T>);
        lua_call(L, 2, 1);
        lua_rawset(L, LUA_REGISTRYINDEX);

        lua_pushlightuserdata(L, &http_op_key<T>::write_response);
        lua_pushvalue(L, -2);
        rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
        lua_pushcfunction(L, socket_write_response<T>);
        lua_call(L, 2, 1);
        lua_rawset(L, LUA_REGISTRYINDEX);

        lua_pushlightuserdata(L, &http_op_key<T>::write_response_continue);
        lua_pushvalue(L, -2);
        rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
        lua_pushcfunction(L, socket_write_response_continue<T>);
        lua_call(L, 2, 1);
        lua_rawset(L, LUA_REGISTRYINDEX);

        lua_pushlightuserdata(L, &http_op_key<T>::write_response_metadata);
        lua_pushvalue(L, -2);
        rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
        lua_pushcfunction(L, socket_write_response_metadata<T>);
        lua_call(L, 2, 1);
        lua_rawset(L, LUA_REGISTRYINDEX);

        lua_pushlightuserdata(L, &http_op_key<T>::write);
        lua_pushvalue(L, -2);
        rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
        lua_pushcfunction(L, socket_write<T>);
        lua_call(L, 2, 1);
        lua_rawset(L, LUA_REGISTRYINDEX);

        lua_pushlightuserdata(L, &http_op_key<T>::write_trailers);
        lua_pushvalue(L, -2);
        rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
        lua_pushcfunction(L, socket_write_trailers<T>);
        lua_call(L, 2, 1);
        lua_rawset(L, LUA_REGISTRYINDEX);

        lua_pushlightuserdata(L, &http_op_key<T>::write_end_of_message);
        lua_pushvalue(L, -2);
        rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
        lua_pushcfunction(L, socket_write_end_of_message<T>);
        lua_call(L, 2, 1);
        lua_rawset(L, LUA_REGISTRYINDEX);

        lua_pushlightuserdata(L, &http_op_key<T>::write_request);
        lua_pushvalue(L, -2);
        rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
        lua_pushcfunction(L, socket_write_request<T>);
        lua_call(L, 2, 1);
        lua_rawset(L, LUA_REGISTRYINDEX);

        lua_pushlightuserdata(L, &http_op_key<T>::write_request_metadata);
        lua_pushvalue(L, -2);
        rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
        lua_pushcfunction(L, socket_write_request_metadata<T>);
        lua_call(L, 2, 1);
        lua_rawset(L, LUA_REGISTRYINDEX);

        lua_pushlightuserdata(L, &http_op_key<T>::read_response);
        lua_pushvalue(L, -2);
        rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
        lua_pushcfunction(L, socket_read_response<T>);
        lua_call(L, 2, 1);
        lua_rawset(L, LUA_REGISTRYINDEX);

        lua_pushlightuserdata(L, &http_op_key<T>::read_some);
        lua_pushvalue(L, -2);
        rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
        lua_pushcfunction(L, socket_read_some<T>);
        lua_call(L, 2, 1);
        lua_rawset(L, LUA_REGISTRYINDEX);
    };
    register_socket(L, hana::type_c<asio::ip::tcp::socket>);
    register_socket(L, hana::type_c<TlsSocket>);

    lua_pushlightuserdata(L, &http_op_key<TlsSocket>::close);
    lua_pushvalue(L, -2);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, socket_close<TlsSocket>);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pop(L, 1);

    lua_pushlightuserdata(L, &http_op_key<asio::ip::tcp::socket>::close);
    lua_pushcfunction(L, socket_close<asio::ip::tcp::socket>);
    lua_rawset(L, LUA_REGISTRYINDEX);
}

} // namespace emilua
