#pragma once

#include <emilua/core.hpp>

namespace emilua {

extern char inbox_key;

void init_actor_module(lua_State* L);

} // namespace emilua
