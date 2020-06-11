#pragma once

#include <deque>

#include <emilua/core.hpp>

namespace emilua {

extern char mutex_key;
extern char mutex_mt_key;

struct mutex_handle
{
    mutex_handle(vm_context& vm_ctx);
    ~mutex_handle();

    std::deque<lua_State*> pending;
    bool locked = false;
    vm_context& vm_ctx;
};

void init_mutex_module(lua_State* L);

} // namespace emilua
