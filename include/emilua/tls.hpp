/* Copyright (c) 2020 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#pragma once

#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <emilua/core.hpp>

namespace emilua {

extern char tls_key;
extern char tls_ctx_mt_key;
extern char tls_socket_mt_key;

struct TlsSocket
{
    TlsSocket(asio::ip::tcp::socket& socket,
              std::shared_ptr<asio::ssl::context> tls_context)
        : socket{std::move(socket), *tls_context}
        , tls_context{std::move(tls_context)}
    {}

    asio::ssl::stream<asio::ip::tcp::socket> socket;
    std::shared_ptr<asio::ssl::context> tls_context;
};

void init_tls(lua_State* L);

} // namespace emilua
