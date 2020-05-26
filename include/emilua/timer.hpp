#pragma once

#include <emilua/core.hpp>

namespace emilua {

result<void, std::bad_alloc> push_sleep_for(lua_State* L);

} // namespace emilua
