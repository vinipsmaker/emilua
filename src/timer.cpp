#include <boost/asio/steady_timer.hpp>

#include <emilua/dispatch_table.hpp>
#include <emilua/fiber.hpp>
#include <emilua/timer.hpp>

namespace emilua {

extern unsigned char sleep_for_bytecode[];
extern std::size_t sleep_for_bytecode_size;

char sleep_for_key;
char timer_key;
static char timer_mt_key;
static char timer_wait_key;

struct sleep_for_operation: public pending_operation
{
    sleep_for_operation(asio::io_context& ctx)
        : pending_operation{/*shared_ownership=*/true}
        , timer{ctx}
        , interrupted{false}
    {}

    void cancel() noexcept override
    {
        try {
            timer.cancel();
        } catch (const boost::system::system_error&) {}
    }

    asio::steady_timer timer;
    bool interrupted;
};

struct handle_type
{
    handle_type(asio::io_context& ctx)
        : timer{ctx}
    {}

    asio::steady_timer timer;
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
            try {
                handle->timer.cancel();
            } catch (const boost::system::system_error&) {}
            return 0;
        },
        1);
    set_interrupter(L);

    vm_ctx->pending_operations.push_back(*handle);

    handle->timer.async_wait(asio::bind_executor(
        vm_ctx->strand_using_defer(),
        [vm_ctx,current_fiber,handle](const boost::system::error_code &ec) {
            std::error_code std_ec = ec;
            if (handle->interrupted && ec == asio::error::operation_aborted)
                std_ec = errc::interrupted;
            vm_ctx->fiber_prologue(
                current_fiber,
                [&]() { push(current_fiber, std_ec); });
            int res = lua_resume(current_fiber, 1);
            vm_ctx->fiber_epilogue(res);
        }
    ));

    return lua_yield(L, 0);
}

static int timer_wait(lua_State* L)
{
    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto handle = reinterpret_cast<handle_type*>(lua_touserdata(L, 1));
    if (!handle || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument);
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &timer_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument);
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }

    lua_pushvalue(L, 1);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto handle = reinterpret_cast<handle_type*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            try {
                handle->timer.cancel();
            } catch (const boost::system::system_error&) {}
            return 0;
        },
        1);
    set_interrupter(L);

    handle->timer.async_wait(asio::bind_executor(
        vm_ctx->strand_using_defer(),
        [vm_ctx,current_fiber](const boost::system::error_code& ec) {
            std::error_code std_ec = ec;
            vm_ctx->fiber_prologue(
                current_fiber,
                [&]() {
                    if (ec == asio::error::operation_aborted) {
                        rawgetp(current_fiber, LUA_REGISTRYINDEX,
                                &fiber_list_key);
                        lua_pushthread(current_fiber);
                        lua_rawget(current_fiber, -2);
                        lua_rawgeti(current_fiber, -1,
                                    FiberDataIndex::INTERRUPTED);
                        bool interrupted = lua_toboolean(current_fiber, -1);
                        lua_pop(current_fiber, 3);
                        if (interrupted)
                            std_ec = errc::interrupted;
                    }
                    push(current_fiber, std_ec);
                });
            int res = lua_resume(current_fiber, 1);
            vm_ctx->fiber_epilogue(res);
        }
    ));

    return lua_yield(L, 0);
}

static int timer_expires_after(lua_State* L)
{
    auto handle = reinterpret_cast<handle_type*>(lua_touserdata(L, 1));
    if (!handle || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument);
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &timer_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument);
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }

    std::chrono::milliseconds::rep msecs = luaL_checknumber(L, 2);

    try {
        auto n = handle->timer.expires_after(std::chrono::milliseconds(msecs));
        lua_pushinteger(L, n);
        return 1;
    } catch (const boost::system::system_error& e) {
        push(L, static_cast<std::error_code>(e.code()));
        return lua_error(L);
    }
}

static int timer_cancel(lua_State* L)
{
    auto handle = reinterpret_cast<handle_type*>(lua_touserdata(L, 1));
    if (!handle || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument);
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &timer_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument);
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }

    try {
        auto n = handle->timer.cancel();
        lua_pushinteger(L, n);
        return 1;
    } catch (const boost::system::system_error& e) {
        push(L, static_cast<std::error_code>(e.code()));
        return lua_error(L);
    }
}

static int timer_mt_index(lua_State* L)
{
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("wait"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX, &timer_wait_key);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("expires_after"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, timer_expires_after);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("cancel"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, timer_cancel);
                    return 1;
                }
            )
        ),
        [](std::string_view /*key*/, lua_State* L) -> int {
            push(L, errc::bad_index);
            lua_pushliteral(L, "index");
            lua_pushvalue(L, 2);
            lua_rawset(L, -3);
            return lua_error(L);
        },
        tostringview(L, 2),
        L
    );
}

static int timer_new(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);
    auto buf = reinterpret_cast<handle_type*>(
        lua_newuserdata(L, sizeof(handle_type))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &timer_mt_key);
    int res = lua_setmetatable(L, -2);
    assert(res); boost::ignore_unused(res);
    new (buf) handle_type{vm_ctx.strand().context()};
    return 1;
}

void init_timer(lua_State* L)
{
    lua_pushlightuserdata(L, &timer_wait_key);
    int res = luaL_loadbuffer(L, reinterpret_cast<char*>(sleep_for_bytecode),
                              sleep_for_bytecode_size, nullptr);
    assert(res == 0); boost::ignore_unused(res);
    lua_pushvalue(L, -1);
    lua_insert(L, -3);
    lua_pushcfunction(L, lua_error);
    lua_pushcfunction(L, timer_wait);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &sleep_for_key);
    lua_insert(L, -2);
    lua_pushcfunction(L, lua_error);
    lua_pushcfunction(L, sleep_for);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &timer_key);
    {
        lua_newtable(L);

        lua_pushliteral(L, "new");
        lua_pushcfunction(L, timer_new);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &timer_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "steady-timer");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, timer_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<handle_type>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);
}

} // namespace emilua
