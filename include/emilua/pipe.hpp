/* Copyright (c) 2022 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#pragma once

#include <emilua/core.hpp>

namespace emilua {

extern char pipe_key;
extern char readable_pipe_mt_key;
extern char writable_pipe_mt_key;

void init_pipe(lua_State* L);

} // namespace emilua
