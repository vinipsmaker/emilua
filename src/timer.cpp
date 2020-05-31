#include <boost/asio/steady_timer.hpp>

#include <emilua/timer.hpp>

namespace emilua {

extern unsigned char sleep_for_bytecode[];
extern std::size_t sleep_for_bytecode_size;

struct handle_type
{
    handle_type(asio::io_context& ctx)
        : timer{ctx}
        , interrupted{false}
    {}

    asio::steady_timer timer;
    bool interrupted;
};

static int sleep_for(lua_State* L)
{
    lua_Integer msecs = luaL_checkinteger(L, 1);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto handle = std::make_shared<handle_type>(vm_ctx->strand().context());
    handle->timer.expires_after(std::chrono::milliseconds(msecs));

    lua_pushlightuserdata(L, handle.get());
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto handle = reinterpret_cast<handle_type*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            handle->interrupted = true;
            handle->timer.cancel();
            return 0;
        },
        1);
    set_interrupter(L);

    handle->timer.async_wait(asio::bind_executor(
        vm_ctx->strand_using_defer(),
        [vm_ctx,current_fiber,handle](const boost::system::error_code &ec) {
            vm_ctx->fiber_prologue(current_fiber);
            std::error_code std_ec = ec;
            if (handle->interrupted && ec == asio::error::operation_aborted)
                std_ec = errc::interrupted;
            push(current_fiber, std_ec).value();
            vm_ctx->reclaim_reserved_zone();
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
