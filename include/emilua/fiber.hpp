/* Copyright (c) 2020 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#pragma once

#include <emilua/core.hpp>

namespace emilua {

extern char yield_reason_is_native_key;

enum FiberStatus: lua_Integer
{
    //RUNNING, //< same as not set/nil
    FINISHED_SUCCESSFULLY = 1,
    FINISHED_WITH_ERROR,
};

struct fiber_handle
{
    fiber_handle(lua_State* fiber)
        : fiber{fiber}
        , interruption_caught{std::in_place_type_t<void>{}}
    {}

    lua_State* fiber;
    bool join_in_progress = false;
    result<bool, void> interruption_caught;
};

void init_fiber_module(lua_State* L);
void print_panic(const lua_State* L, bool is_main, std::string_view error,
                 std::string_view stacktrace);

int set_current_traceback(lua_State* L);

} // namespace emilua
