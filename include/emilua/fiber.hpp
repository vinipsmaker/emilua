#pragma once

#include <emilua/core.hpp>

#define EMILUA_IMPL_INITIAL_FIBER_DATA_CAPACITY 8
#define EMILUA_IMPL_INITIAL_MODULE_FIBER_DATA_CAPACITY 6

// EMILUA_IMPL_INITIAL_MODULE_FIBER_DATA_CAPACITY currently takes into
// consideration:
//
// * STACK
// * LEAF
// * CONTEXT
// * INTERRUPTION_DISABLED
// * JOINER
// * STATUS

namespace emilua {

extern char fiber_list_key;
extern char yield_reason_is_native_key;

enum FiberDataIndex: lua_Integer
{
    JOINER = 1,
    STATUS,
    SUSPENSION_DISALLOWED,
    LOCAL_STORAGE,
    STACKTRACE,

    // data used by the interruption system {{{
    INTERRUPTION_DISABLED,
    INTERRUPTED,
    INTERRUPTER,
    USER_HANDLE, //< "augmented joiner"
    // }}}

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
