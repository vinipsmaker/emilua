#include <boost/asio/steady_timer.hpp>

#include <emilua/timer.hpp>

namespace emilua {

extern unsigned char sleep_for_bytecode[];
extern std::size_t sleep_for_bytecode_size;

static int sleep_for(lua_State* L)
{
    // TODO: handle this_fiber.disable_interruption state
    lua_Integer msecs = luaL_checkinteger(L, 1);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();

    auto t = std::make_shared<asio::steady_timer>(vm_ctx->strand().context());
    t->expires_after(std::chrono::milliseconds(msecs));
    t->async_wait(asio::bind_executor(
        vm_ctx->strand_using_defer(),
        [vm_ctx,current_fiber,t](const boost::system::error_code &ec) {
            if (!vm_ctx->valid())
                return;

            // TODO: fiber interruption

            vm_ctx->fiber_prologue(current_fiber);
            vm_ctx->enable_reserved_zone();

            if (result<void, std::bad_alloc> r = push(current_fiber, ec) ; !r) {
                vm_ctx->fiber_epilogue(LUA_ERRMEM);
                return;
            }

            vm_ctx->reclaim_reserved_zone_or_close();
            if (!vm_ctx->valid())
                return;

            int res = lua_resume(current_fiber, 1);
            vm_ctx->fiber_epilogue(res);
        }
    ));

    return lua_yield(L, 0);
}

result<void, std::bad_alloc> push_sleep_for(lua_State* L)
{
    if (!lua_checkstack(L, 3))
        return std::bad_alloc{};

    int res = luaL_loadbuffer(L, reinterpret_cast<char*>(sleep_for_bytecode),
                              sleep_for_bytecode_size, nullptr);
    assert(res != LUA_ERRSYNTAX);
    if (res == LUA_ERRMEM)
        return std::bad_alloc{};

    assert(res == 0);
    lua_pushcfunction(L, lua_error);
    lua_pushcfunction(L, sleep_for);
    lua_call(L, 2, 1);
    return outcome::success();
}

} // namespace emilua
