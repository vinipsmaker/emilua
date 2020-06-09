#include <deque>

#include <fmt/format.h>

#include <emilua/dispatch_table.hpp>
#include <emilua/mutex.hpp>

namespace emilua {

char mutex_key;
static char mutex_mt_key;

struct mutex_handle
{
    mutex_handle(vm_context& vm_ctx)
        : vm_ctx{vm_ctx}
    {}

    ~mutex_handle()
    {
        constexpr auto spec{FMT_STRING(
            "No scheduled fibers remaining to unlock mutex {}"
        )};
        if (pending.size() != 0)
            vm_ctx.notify_deadlock(fmt::format(spec, static_cast<void*>(this)));
    }

    std::deque<lua_State*> pending;
    bool locked = false;
    vm_context& vm_ctx;
};

static int mutex_lock(lua_State* L)
{
    auto handle = reinterpret_cast<mutex_handle*>(lua_touserdata(L, 1));
    if (!handle || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument).value();
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &mutex_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument).value();
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }

    auto& vm_ctx = handle->vm_ctx;
    EMILUA_CHECK_SUSPEND_ALLOWED_ASSUMING_INTERRUPTION_DISABLED(vm_ctx, L);

    if (handle->locked) {
        handle->pending.emplace_back(vm_ctx.current_fiber());
        return lua_yield(L, 0);
    }

    handle->locked = true;
    return 0;
}

static int mutex_unlock(lua_State* L)
{
    auto handle = reinterpret_cast<mutex_handle*>(lua_touserdata(L, 1));
    if (!handle || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument).value();
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &mutex_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument).value();
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }

    if (!handle->locked) {
        push(L, std::errc::operation_not_permitted).value();
        return lua_error(L);
    }

    if (handle->pending.size() == 0) {
        handle->locked = false;
        return 0;
    }

    auto vm_ctx = handle->vm_ctx.shared_from_this();
    auto next = handle->pending.front();
    handle->pending.pop_front();
    vm_ctx->strand().post([vm_ctx,next]() {
        vm_ctx->fiber_resume_trivial(next);
    }, std::allocator<void>{});
    return 0;
}

static int mutex_mt_index(lua_State* L)
{
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("unlock"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, mutex_unlock);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("lock"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, mutex_lock);
                    return 1;
                }
            )
        ),
        [](std::string_view /*key*/, lua_State* L) -> int {
            push(L, errc::bad_index).value();
            lua_pushliteral(L, "index");
            lua_pushvalue(L, 2);
            lua_rawset(L, -3);
            return lua_error(L);
        },
        tostringview(L, 2),
        L
    );
}

static int mutex_new(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);
    auto buf = reinterpret_cast<mutex_handle*>(
        lua_newuserdata(L, sizeof(mutex_handle))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &mutex_mt_key);
    int res = lua_setmetatable(L, -2);
    assert(res); boost::ignore_unused(res);
    new (buf) mutex_handle{vm_ctx};
    return 1;
}

void init_mutex_module(lua_State* L)
{
    lua_pushlightuserdata(L, &mutex_key);
    lua_newtable(L);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushlightuserdata(L, nullptr);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(
            L,
            [](lua_State* L) -> int {
                auto key = tostringview(L, 2);
                if (key == "new") {
                    lua_pushcfunction(L, mutex_new);
                    return 1;
                } else {
                    push(L, errc::bad_index).value();
                    lua_pushliteral(L, "index");
                    lua_pushvalue(L, 2);
                    lua_rawset(L, -3);
                    return lua_error(L);
                }
            });
        lua_rawset(L, -3);

        lua_pushliteral(L, "__newindex");
        lua_pushcfunction(
            L,
            [](lua_State* L) -> int {
                push(L, std::errc::operation_not_permitted).value();
                return lua_error(L);
            });
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &mutex_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "mutex");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, mutex_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<mutex_handle>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);
}

} // namespace emilua
