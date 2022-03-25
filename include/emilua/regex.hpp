/* Copyright (c) 2022 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#pragma once

#include <emilua/core.hpp>

namespace emilua {

extern char regex_key;
extern char regex_mt_key;

void init_regex(lua_State* L);
const std::error_category& regex_category() noexcept;

} // namespace emilua
