/* Copyright (c) 2022 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#include <boost/beast/websocket.hpp>

#include <emilua/dispatch_table.hpp>
#include <emilua/byte_span.hpp>
#include <emilua/websocket.hpp>
#include <emilua/http.hpp>
#include <emilua/tls.hpp>
#include <emilua/ip.hpp>

namespace emilua {

extern unsigned char websocket_op_bytecode[];
extern std::size_t websocket_op_bytecode_size;

char websocket_key;

static char websocket_mt_key;
static char websocket_tls_mt_key;

template<class T>
struct websocket_op_key
{
    static char close;
    static char write;
    static char read_some;
};

template<class T> char websocket_op_key<T>::close;
template<class T> char websocket_op_key<T>::write;
template<class T> char websocket_op_key<T>::read_some;

template<class T>
struct get_websocket_mt_key;

template<>
struct get_websocket_mt_key<asio::ip::tcp::socket>
{
    static void* get()
    {
        return &websocket_mt_key;
    }
};

template<>
struct get_websocket_mt_key<TlsSocket>
{
    static void* get()
    {
        return &websocket_tls_mt_key;
    }
};

template<class T>
using websocket_stream = boost::beast::websocket::stream<
    T,
#if EMILUA_CONFIG_ENABLE_WEBSOCKET_PERMESSAGE_DEFLATE
    /*deflateSupported=*/true
#else
    /*deflateSupported=*/false
#endif
>;

template<class T>
struct websocket_client_new_operation: public pending_operation
{
    websocket_client_new_operation(websocket_stream<T>* ws)
        : pending_operation{/*shared_ownership=*/true}
        , interrupted{false}
        , websocket{ws}
    {}

    void cancel() noexcept override
    {
        try {
            boost::beast::get_lowest_layer(*websocket).cancel();
        } catch (const boost::system::system_error&) {}
    }

    boost::beast::websocket::response_type response;
    bool interrupted;
    websocket_stream<T>* websocket;
};

inline
boost::beast::http::request<boost::beast::http::empty_body>
to_beast_request(const Request& src)
{
    boost::beast::http::request<boost::beast::http::empty_body> dest;
    dest.method_string(src.method());
    dest.target(src.target());
    for (auto& hdr: src.headers()) {
        dest.insert(hdr.first, hdr.second);
    }
    return dest;
}

static int websocket_is_websocket_upgrade(lua_State* L)
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
    int res = boost::beast::websocket::is_upgrade(to_beast_request(**m));
    lua_pushboolean(L, res);
    return 1;
}

template<class T>
inline int websocket_new_client(lua_State* L, int nargs)
{
    if (nargs < 3) {
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    }
    if (lua_type(L, 2) != LUA_TSTRING) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    if (lua_type(L, 3) != LUA_TSTRING) {
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    }
    if (nargs > 3) {
        if (lua_type(L, 4) != LUA_TTABLE) {
            push(L, std::errc::invalid_argument, "arg", 4);
            return lua_error(L);
        }
    }

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto sock = reinterpret_cast<T*>(lua_touserdata(L, 1));
    auto host = tostringview(L, 2);
    auto request_target = tostringview(L, 3);

    auto ws = std::make_shared<websocket_stream<T>>(std::move(*sock));
    lua_pushnil(L);
    lua_setmetatable(L, 1);
    sock->T::~T();

    auto pending_op = std::make_shared<websocket_client_new_operation<T>>(
        ws.get());
    vm_ctx->pending_operations.push_back(*pending_op);

    lua_pushlightuserdata(L, pending_op.get());
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto op = reinterpret_cast<websocket_client_new_operation<T>*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            op->interrupted = true;
            try {
                boost::beast::get_lowest_layer(*op->websocket).cancel();
            } catch (const boost::system::system_error&) {}
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    if (nargs > 3) {
        boost::beast::http::fields extra_headers;
        lua_pushnil(L);
        while (lua_next(L, 4) != 0) {
            if (lua_type(L, -2) != LUA_TSTRING) {
                lua_pop(L, 1);
                continue;
            }

            auto key = tostringview(L, -2);
            switch (lua_type(L, -1)) {
            case LUA_TSTRING:
                extra_headers.set(key, tostringview(L, -1));
                break;
            case LUA_TTABLE: {
                if (lua_getmetatable(L, -1)) {
                    lua_pop(L, 1);
                    break;
                }

                for (int i = 1 ;; ++i) {
                    lua_rawgeti(L, -1, i);
                    if (lua_type(L, -1) != LUA_TSTRING) {
                        lua_pop(L, 1);
                        break;
                    }

                    extra_headers.insert(key, tostringview(L, -1));
                    lua_pop(L, 1);
                }
                break;
            }
            default: break;
            }
            lua_pop(L, 1);
        }

        ws->set_option(boost::beast::websocket::stream_base::decorator(
            [
                extra_headers=std::move(extra_headers)
            ](boost::beast::websocket::request_type& req) {
                for (auto& header: extra_headers) {
                    req.insert(header.name_string(), header.value());
                }
            }
        ));
    }

    ws->async_handshake(
        pending_op->response, host, request_target,
        asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [ws,pending_op,vm_ctx,current_fiber](boost::system::error_code ec) {
                auto websocket_pusher = [&ws](lua_State* L) {
                    using U = websocket_stream<T>;
                    auto s = reinterpret_cast<std::shared_ptr<U>*>(
                        lua_newuserdata(L, sizeof(std::shared_ptr<U>)));
                    rawgetp(L, LUA_REGISTRYINDEX,
                            get_websocket_mt_key<T>::get());
                    setmetatable(L, -2);
                    new (s) std::shared_ptr<U>{ws};
                };

                // FIXME: If server's reply includes multiple headers with the
                // same key then this code will ignore all but one of these
                // headers.
                auto response_pusher = [&pending_op](lua_State* L) {
                    lua_newtable(L);
                    for (auto& header: pending_op->response) {
                        std::string key(header.name_string());
                        for (char& ch: key)
                            ch = std::tolower(ch);
                        push(L, key);
                        push(L, header.value());
                        lua_rawset(L, -3);
                    }
                };

                vm_ctx->pending_operations.erase(
                    vm_ctx->pending_operations.s_iterator_to(*pending_op));

                std::error_code std_ec = ec;
                if (pending_op->interrupted &&
                    ec == asio::error::operation_aborted) {
                    std_ec = errc::interrupted;
                }
                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(
                                std_ec, websocket_pusher, response_pusher))));
            }
        )
    );

    return lua_yield(L, 0);
}

template<class T>
inline int websocket_new_server(lua_State* L, int nargs)
{
    // FIXME: If the Lua VM crashes then the WebSocket object will be kept alive
    // (given the VM will finalize a shared_ptr and not the contained object
    // itself). Pending operations will prevent the execution context from
    // quitting. A `pending_operation` object should be registered with the VM.
    //
    // FIXME: No interrupter is being set here.
    //
    // FIXME: Conversion from/to beast::http::request not always take into
    // account multiple headers with same header key.
    //
    // FIXME: These same mistakes happen throughout other functions in this
    // file.

    if (nargs < 2) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    if (!lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &http_request_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    if (nargs > 2) {
        if (lua_type(L, 3) != LUA_TTABLE) {
            push(L, std::errc::invalid_argument, "arg", 3);
            return lua_error(L);
        }
    }

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto sock = reinterpret_cast<HttpSocket<T>*>(lua_touserdata(L, 1));
    auto req = **reinterpret_cast<std::shared_ptr<Request>*>(
        lua_touserdata(L, 2));

    auto ws = std::make_shared<websocket_stream<T>>(
        std::move(sock->socket.next_layer()));
    lua_pushnil(L);
    lua_setmetatable(L, 1);
    sock->HttpSocket<T>::~HttpSocket<T>();

    // TODO: interrupter

    if (nargs > 2) {
        boost::beast::http::fields extra_headers;
        lua_pushnil(L);
        while (lua_next(L, 3) != 0) {
            if (lua_type(L, -2) != LUA_TSTRING) {
                lua_pop(L, 1);
                continue;
            }

            auto key = tostringview(L, -2);
            switch (lua_type(L, -1)) {
            case LUA_TSTRING:
                extra_headers.set(key, tostringview(L, -1));
                break;
            case LUA_TTABLE: {
                if (lua_getmetatable(L, -1)) {
                    lua_pop(L, 1);
                    break;
                }

                for (int i = 1 ;; ++i) {
                    lua_rawgeti(L, -1, i);
                    if (lua_type(L, -1) != LUA_TSTRING) {
                        lua_pop(L, 1);
                        break;
                    }

                    extra_headers.insert(key, tostringview(L, -1));
                    lua_pop(L, 1);
                }
                break;
            }
            default: break;
            }
            lua_pop(L, 1);
        }

        ws->set_option(boost::beast::websocket::stream_base::decorator(
            [
                extra_headers=std::move(extra_headers)
            ](boost::beast::websocket::response_type& res) {
                for (auto& header: extra_headers) {
                    res.insert(header.name_string(), header.value());
                }
            }
        ));
    }

    ws->async_accept(
        to_beast_request(req),
        asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [ws,vm_ctx,current_fiber](boost::system::error_code ec) {
                auto websocket_pusher = [&ws](lua_State* L) {
                    using U = websocket_stream<T>;
                    auto s = reinterpret_cast<std::shared_ptr<U>*>(
                        lua_newuserdata(L, sizeof(std::shared_ptr<U>)));
                    rawgetp(L, LUA_REGISTRYINDEX,
                            get_websocket_mt_key<T>::get());
                    setmetatable(L, -2);
                    new (s) std::shared_ptr<U>{ws};
                };

                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(ec, websocket_pusher))));
            }
        )
    );

    return lua_yield(L, 0);
}

static int websocket_new(lua_State* L)
{
    int nargs = lua_gettop(L);

    switch (lua_type(L, 1)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    case LUA_TUSERDATA:
        if (!lua_getmetatable(L, 1)) {
            push(L, std::errc::invalid_argument, "arg", 1);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_socket_mt_key);
        if (lua_rawequal(L, -1, -2)) {
            return websocket_new_client<asio::ip::tcp::socket>(L, nargs);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &tls_socket_mt_key);
        if (lua_rawequal(L, -1, -3)) {
            return websocket_new_client<TlsSocket>(L, nargs);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &http_socket_mt_key);
        if (lua_rawequal(L, -1, -4)) {
            return websocket_new_server<asio::ip::tcp::socket>(L, nargs);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &https_socket_mt_key);
        if (lua_rawequal(L, -1, -4)) {
            return websocket_new_server<TlsSocket>(L, nargs);
        }
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
}

template<class T>
static int websocket_close(lua_State* L)
{
    lua_settop(L, 3);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    using U = websocket_stream<T>;
    auto s = reinterpret_cast<std::shared_ptr<U>*>(lua_touserdata(L, 1));
    if (!s || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, get_websocket_mt_key<T>::get());
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::beast::websocket::close_reason close_reason;

    switch (lua_type(L, 2)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    case LUA_TNIL:
        close_reason.code = 1005;
        break;
    case LUA_TNUMBER:
        close_reason.code = lua_tointeger(L, 2);
    }

    switch (lua_type(L, 3)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    case LUA_TNIL:
        break;
    case LUA_TSTRING:
        close_reason.reason = tostringview(L, 3);
    }

    lua_pushvalue(L, 1);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto s = reinterpret_cast<std::shared_ptr<U>*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            boost::system::error_code ignored_ec;
            boost::beast::get_lowest_layer(**s).close(ignored_ec);
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    (*s)->async_close(close_reason, asio::bind_executor(
        vm_ctx->strand_using_defer(),
        [vm_ctx,current_fiber,s](const boost::system::error_code& ec) {
            boost::system::error_code ec2;
            boost::beast::get_lowest_layer(**s).close(ec2);
            if (ec) ec2 = ec;

            auto opt_args = vm_context::options::arguments;
            vm_ctx->fiber_resume(
                current_fiber,
                hana::make_set(
                    vm_context::options::auto_detect_interrupt,
                    hana::make_pair(opt_args, hana::make_tuple(ec2))));
        }
    ));

    return lua_yield(L, 0);
}

template<class T>
static int websocket_write(lua_State* L)
{
    lua_settop(L, 3);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    using U = websocket_stream<T>;
    auto s = reinterpret_cast<std::shared_ptr<U>*>(lua_touserdata(L, 1));
    if (!s || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, get_websocket_mt_key<T>::get());
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    std::shared_ptr<unsigned char[]> data;
    std::size_t size;
    bool is_binary;

    switch (lua_type(L, 2)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    case LUA_TUSERDATA: {
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
        data = bs->data;
        size = bs->size;
        is_binary = true;
    }
        break;
    case LUA_TSTRING: {
        const char* input = lua_tolstring(L, 2, &size);
        data.reset(new unsigned char[size]);
        std::memcpy(data.get(), input, size);
        is_binary = false;
    }
        break;
    }

    switch (lua_type(L, 3)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    case LUA_TNIL:
        break;
    case LUA_TSTRING: {
        auto type = tostringview(L, 3);
        if (type == "binary") {
            is_binary = true;
        } else if (type == "text") {
            is_binary = false;
        } else if (type != "auto") {
            push(L, std::errc::invalid_argument, "arg", 3);
            return lua_error(L);
        }
    }
    }

    lua_pushvalue(L, 1);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto s = reinterpret_cast<std::shared_ptr<U>*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            boost::system::error_code ignored_ec;
            boost::beast::get_lowest_layer(**s).close(ignored_ec);
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    (*s)->binary(is_binary);
    (*s)->async_write(asio::buffer(data.get(), size), asio::bind_executor(
        vm_ctx->strand_using_defer(),
        [data,vm_ctx,current_fiber,s](const boost::system::error_code& ec,
                                      std::size_t /*bytes_transferred*/) {
            boost::ignore_unused(s);

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

template<class T>
static int websocket_read_some(lua_State* L)
{
    lua_settop(L, 2);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    using U = websocket_stream<T>;
    auto s = reinterpret_cast<std::shared_ptr<U>*>(lua_touserdata(L, 1));
    if (!s || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, get_websocket_mt_key<T>::get());
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    if (lua_type(L, 2) != LUA_TUSERDATA) {
        push(L, std::errc::invalid_argument, "arg", 2);
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
            auto s = reinterpret_cast<std::shared_ptr<U>*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            boost::system::error_code ignored_ec;
            boost::beast::get_lowest_layer(**s).close(ignored_ec);
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    (*s)->async_read_some(
        asio::buffer(bs->data.get(), bs->size),
        asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [buf=bs->data,vm_ctx,current_fiber,s](
                const boost::system::error_code& ec, std::size_t bytes_read
            ) {
                boost::ignore_unused(buf);

                std::string_view status;
                if (!(*s)->is_message_done())
                    status = "partial";
                else if ((*s)->got_binary())
                    status = "binary";
                else
                    status = "text";
                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(ec, bytes_read, status))));
            }
        )
    );

    return lua_yield(L, 0);
}

template<class T>
static int websocket_mt_index(lua_State* L)
{
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("close"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX, &websocket_op_key<T>::close);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("write"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX, &websocket_op_key<T>::write);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("read_some"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX,
                            &websocket_op_key<T>::read_some);
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

template<class T>
static void register_socket(lua_State* L)
{
    lua_pushlightuserdata(L, get_websocket_mt_key<T>::get());
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "websocket");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, websocket_mt_index<T>);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<std::shared_ptr<websocket_stream<T>>>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &websocket_op_key<T>::close);
    lua_pushvalue(L, -2);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, websocket_close<T>);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &websocket_op_key<T>::write);
    lua_pushvalue(L, -2);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, websocket_write<T>);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &websocket_op_key<T>::read_some);
    lua_pushvalue(L, -2);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, websocket_read_some<T>);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);
}

void init_websocket(lua_State* L)
{
    int res = luaL_loadbuffer(L, reinterpret_cast<char*>(websocket_op_bytecode),
                              websocket_op_bytecode_size, nullptr);
    assert(res == 0); boost::ignore_unused(res);

    lua_pushlightuserdata(L, &websocket_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/2);

        lua_pushliteral(L, "new");
        {
            lua_pushvalue(L, -4);
            rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
            lua_pushcfunction(L, websocket_new);
            lua_call(L, 2, 1);
        }
        lua_rawset(L, -3);

        lua_pushliteral(L, "is_websocket_upgrade");
        lua_pushcfunction(L, websocket_is_websocket_upgrade);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    register_socket<asio::ip::tcp::socket>(L);
    register_socket<TlsSocket>(L);
    lua_pop(L, 1);
}

} // namespace emilua
