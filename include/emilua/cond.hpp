#pragma once

#include <emilua/core.hpp>

namespace emilua {

extern char cond_key;

void init_cond_module(lua_State* L);

} // namespace emilua
