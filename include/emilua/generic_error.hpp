/* Copyright (c) 2023 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#pragma once

#include <emilua/core.hpp>

namespace emilua {

extern char generic_error_key;

// returns 0 on error
int posix_errno_code_from_name(std::string_view name);

void init_generic_error(lua_State* L);

} // namespace emilua
