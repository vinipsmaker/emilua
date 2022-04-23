/* Copyright (c) 2022 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#pragma once

#include <emilua/core.hpp>

namespace emilua {

extern char file_descriptor_mt_key;

// When *this == INVALID_FILE_DESCRIPTOR: raise EBUSY
using file_descriptor_handle = int;

constexpr file_descriptor_handle INVALID_FILE_DESCRIPTOR = -1;

void init_file_descriptor(lua_State* L);

} // namespace emilua
