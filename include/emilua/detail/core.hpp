#pragma once

#include <emilua/core.hpp>

namespace emilua {
namespace detail {

extern char fiber_list_key;
extern char context_key;
extern char error_code_key;
extern char error_category_key;

enum FiberDataIndex: lua_Integer
{
    // data only avaiable for modules:
    STACK,
    LEAF,
    CONTEXT
};

} // namespace detail
} // namespace emilua
