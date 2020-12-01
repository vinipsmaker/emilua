#pragma once

#include <emilua/core.hpp>

namespace emilua {

extern char sleep_for_key;
extern char timer_key;

void init_timer(lua_State* L);

} // namespace emilua
