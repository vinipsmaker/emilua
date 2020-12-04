#pragma once

#include <emilua/core.hpp>

namespace emilua {

extern char ip_key;
extern char ip_address_mt_key;
extern char ip_tcp_socket_mt_key;
extern char ip_tcp_acceptor_mt_key;

void init_ip(lua_State* L);

} // namespace emilua
