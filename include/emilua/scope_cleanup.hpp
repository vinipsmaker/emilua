/* Copyright (c) 2020 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#pragma once

#include <emilua/core.hpp>

namespace emilua {

extern char scope_cleanup_handlers_key;

void init_scope_cleanup_module(lua_State* L);
void init_new_coro_or_fiber_scope(lua_State* L, lua_State* from);

// * Must be called through `lua_call` or from lua context.
// * It doesn't sanitize environment. Matching push()/pop() must be ensured
//   externally.
int unsafe_scope_push(lua_State* L);
int unsafe_scope_pop(lua_State* L);
int terminate_vm_with_cleanup_error(lua_State* L);
int root_scope(lua_State* L);

} // namespace emilua
