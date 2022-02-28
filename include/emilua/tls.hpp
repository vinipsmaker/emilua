/* Copyright (c) 2020 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#pragma once

#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <boost/beast/websocket/teardown.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>

#include <emilua/core.hpp>

namespace emilua {

extern char tls_key;
extern char tls_context_mt_key;
extern char tls_socket_mt_key;

class TlsSocket
    : private std::shared_ptr<asio::ssl::context>
    , public boost::beast::ssl_stream<asio::ip::tcp::socket>
{
public:
    // TODO: fix bug in Boost.Http and remove this workaround
    using lowest_layer_type = asio::ip::tcp::socket;

    TlsSocket(asio::ip::tcp::socket& socket,
              std::shared_ptr<asio::ssl::context> tls_context)
        : std::shared_ptr<asio::ssl::context>{std::move(tls_context)}
        , boost::beast::ssl_stream<asio::ip::tcp::socket>{
            std::move(socket),
            *static_cast<std::shared_ptr<asio::ssl::context>&>(*this)}
    {}

    TlsSocket(TlsSocket&&) = default;

    // TODO: fix bug in Boost.Http and remove this workaround
    lowest_layer_type& lowest_layer()
    {
        return next_layer();
    }

    // TODO: fix bug in Boost.Http and remove this workaround
    bool is_open() const
    {
        return next_layer().is_open();
    }

    std::shared_ptr<asio::ssl::context>& tls_context()
    {
        return *this;
    }
};

template<class TeardownHandler>
inline void async_teardown(boost::beast::role_type role, TlsSocket& stream,
                           TeardownHandler&& handler)
{
    using boost::beast::websocket::async_teardown;
    async_teardown(
        role,
        static_cast<boost::beast::ssl_stream<asio::ip::tcp::socket>&>(stream),
        std::forward<TeardownHandler>(handler));
}

void init_tls(lua_State* L);

} // namespace emilua
