/* Copyright (c) 2020 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#pragma once

#include <emilua/core.hpp>

namespace emilua {

enum class json_errc {
    result_out_of_range = 1,
    too_many_levels,
    array_too_long,
    cycle_exists,
};

const std::error_category& json_category();

inline std::error_code make_error_code(json_errc e)
{
    return std::error_code{static_cast<int>(e), json_category()};
}

extern char json_key;

void init_json_module(lua_State* L);

} // namespace emilua

template<>
struct std::is_error_code_enum<emilua::json_errc>: std::true_type {};
