/* Copyright (c) 2020 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#pragma once

#include <emilua/core.hpp>

namespace emilua {

extern char sleep_for_key;
extern char timer_key;

void init_timer(lua_State* L);

} // namespace emilua
