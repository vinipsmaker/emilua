/* Copyright (c) 2022 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#pragma once

#include <emilua/core.hpp>

namespace emilua {

extern char var_args__retval1_to_error__fwd_retval2__key;
extern char var_args__retval1_to_error__key;
extern char var_args__retval1_to_error__fwd_retval234__key;
extern char var_args__retval1_to_error__fwd_retval23__key;

void init_async_base(lua_State* L);

} // namespace emilua
