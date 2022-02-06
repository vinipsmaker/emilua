/* Copyright (c) 2020, 2021 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#pragma once

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>

#include <emilua/socket_base.hpp>

namespace emilua {

extern char ip_key;
extern char ip_address_mt_key;
extern char ip_tcp_socket_mt_key;
extern char ip_tcp_acceptor_mt_key;
extern char ip_tcp_resolver_mt_key;

using tcp_socket = Socket<asio::ip::tcp::socket>;
using udp_socket = Socket<asio::ip::udp::socket>;

void init_ip(lua_State* L);

} // namespace emilua
