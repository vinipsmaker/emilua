#pragma once

#include <emilua/core.hpp>

namespace emilua {

extern char mutex_key;

void init_mutex_module(lua_State* L);

} // namespace emilua
