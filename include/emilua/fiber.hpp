#pragma once

#include <emilua/core.hpp>

namespace emilua {

extern char fiber_list_key;

enum FiberDataIndex: lua_Integer
{
    JOINER = 1,
    STATUS,
    SUSPENSION_DISALLOWED,
    LOCAL_STORAGE,

    // data only avaiable for modules:
    STACK,
    LEAF,
    CONTEXT
};

enum FiberStatus: lua_Integer
{
    //RUNNING, //< same as not set/nil
    FINISHED_SUCCESSFULLY = 1,
    FINISHED_WITH_ERROR,
};

void init_fiber_module(lua_State* L);
void print_panic(const lua_State* L, bool is_main, std::string_view error,
                 std::string_view stacktrace);

} // namespace emilua
