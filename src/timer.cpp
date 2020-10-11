#include <boost/asio/steady_timer.hpp>

#include <emilua/timer.hpp>

namespace emilua {

extern unsigned char sleep_for_bytecode[];
extern std::size_t sleep_for_bytecode_size;

char sleep_for_key;

struct sleep_for_operation: public pending_operation
{
    sleep_for_operation(asio::io_context& ctx)
        : pending_operation{/*shared_ownership=*/true}
        , timer{ctx}
        , interrupted{false}
    {}

    void cancel() noexcept override
    {
        timer.cancel();
    }

    asio::steady_timer timer;
    bool interrupted;
};

static int sleep_for(lua_State* L)
{
    lua_Integer msecs = luaL_checkinteger(L, 1);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto handle = std::make_shared<sleep_for_operation>(
        vm_ctx->strand().context());
    handle->timer.expires_after(std::chrono::milliseconds(msecs));

    lua_pushlightuserdata(L, handle.get());
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto handle = reinterpret_cast<sleep_for_operation*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            handle->interrupted = true;
            handle->timer.cancel();
            return 0;
        },
        1);
    set_interrupter(L);

    vm_ctx->pending_operations.push_back(*handle);

    handle->timer.async_wait(asio::bind_executor(
        vm_ctx->strand_using_defer(),
        [vm_ctx,current_fiber,handle](const boost::system::error_code &ec) {
            vm_ctx->fiber_prologue(current_fiber);
            std::error_code std_ec = ec;
            if (handle->interrupted && ec == asio::error::operation_aborted)
                std_ec = errc::interrupted;
            push(current_fiber, std_ec);
            vm_ctx->reclaim_reserved_zone();
            int res = lua_resume(current_fiber, 1);
            vm_ctx->fiber_epilogue(res);
        }
    ));

    return lua_yield(L, 0);
}

void init_timer(lua_State* L)
{
    lua_pushlightuserdata(L, &sleep_for_key);
    int res = luaL_loadbuffer(L, reinterpret_cast<char*>(sleep_for_bytecode),
                              sleep_for_bytecode_size, nullptr);
    assert(res == 0); boost::ignore_unused(res);
    lua_pushcfunction(L, lua_error);
    lua_pushcfunction(L, sleep_for);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);
}

} // namespace emilua
