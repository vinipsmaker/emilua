/* Copyright (c) 2022 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#pragma once

#include <boost/asio/local/datagram_protocol.hpp>
#include <boost/asio/local/stream_protocol.hpp>

#include <emilua/socket_base.hpp>

namespace emilua {

extern char unix_key;
extern char unix_datagram_socket_mt_key;
extern char unix_stream_socket_mt_key;

using unix_datagram_socket = Socket<asio::local::datagram_protocol::socket>;
using unix_stream_socket = Socket<asio::local::stream_protocol::socket>;

void init_unix(lua_State* L);

} // namespace emilua
