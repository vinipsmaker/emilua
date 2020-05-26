#include <cstdio>

#include <emilua/fiber.hpp>
#include <emilua/dispatch_table.hpp>

#include <fmt/format.h>

#include <boost/hana/maximum.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/plus.hpp>

namespace asio = boost::asio;
namespace hana = boost::hana;

namespace emilua {

extern unsigned char fiber_join_bytecode[];
extern std::size_t fiber_join_bytecode_size;

char fiber_list_key;
static char fiber_mt_key;
static char fiber_join_key;

struct fiber_handle
{
    fiber_handle(lua_State* fiber)
        : fiber{fiber}
    {}

    lua_State* fiber;
};

static int fiber_join(lua_State* L)
{
    // TODO: handle interruption

    auto& vm_ctx = get_vm_context(L);
    auto handle = reinterpret_cast<fiber_handle*>(lua_touserdata(L, 1));
    if (!handle || !lua_getmetatable(L, 1)) {
        push(L, std::errc::argument_out_of_domain);
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 1);
        lua_rawset(L, -3);
        lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &fiber_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::argument_out_of_domain);
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 1);
        lua_rawset(L, -3);
        lua_error(L);
    }

    if (!handle->fiber) {
        push(L, std::errc::invalid_argument);
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 1);
        lua_rawset(L, -3);
        lua_error(L);
    }

    if (handle->fiber == L) {
        push(L, std::errc::resource_deadlock_would_occur);
        lua_error(L);
    }

    if (!lua_checkstack(handle->fiber, 3) ||
        !lua_checkstack(vm_ctx.current_fiber(), 1)) {
        vm_ctx.notify_errmem();
        return lua_yield(L, 0);
    }

    rawgetp(handle->fiber, LUA_REGISTRYINDEX, &fiber_list_key);
    lua_pushthread(handle->fiber);
    lua_rawget(handle->fiber, -2);
    lua_xmove(handle->fiber, L, 1);
    lua_pop(handle->fiber, 1);

    lua_rawgeti(L, -1, FiberDataIndex::STATUS);
    int status_type = lua_type(L, -1);
    if (status_type == LUA_TNIL) {
        // Still running
        lua_pushthread(vm_ctx.current_fiber());
        lua_xmove(vm_ctx.current_fiber(), L, 1);
        lua_rawseti(L, -3, FiberDataIndex::JOINER);
        handle->fiber = nullptr;
        return lua_yield(L, 0);
    } else { assert(status_type == LUA_TNUMBER);
        // Finished
        lua_Integer status = lua_tointeger(L, -1);
        switch (status) {
        case FiberStatus::FINISHED_SUCCESSFULLY: {
            lua_pushboolean(L, 1);
            int nret = lua_gettop(handle->fiber);
            if (!lua_checkstack(L, nret)) {
                vm_ctx.notify_errmem();
                return lua_yield(L, 0);
            }
            lua_xmove(handle->fiber, L, nret);
            rawgetp(handle->fiber, LUA_REGISTRYINDEX, &fiber_list_key);
            lua_pushthread(handle->fiber);
            lua_pushnil(handle->fiber);
            lua_rawset(handle->fiber, -3);
            handle->fiber = nullptr;
            // TODO (?): force a full GC round now
            return nret + 1;
        }
        case FiberStatus::FINISHED_WITH_ERROR:
            lua_xmove(handle->fiber, L, 1);
            rawgetp(handle->fiber, LUA_REGISTRYINDEX, &fiber_list_key);
            lua_pushthread(handle->fiber);
            lua_pushnil(handle->fiber);
            lua_rawset(handle->fiber, -3);
            handle->fiber = nullptr;
            return lua_error(L);
        default: assert(false);
        }
    }
    return 0;
}

static int fiber_detach(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);
    auto handle = reinterpret_cast<fiber_handle*>(lua_touserdata(L, 1));
    if (!handle || !lua_getmetatable(L, 1)) {
        push(L, std::errc::argument_out_of_domain);
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 1);
        lua_rawset(L, -3);
        lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &fiber_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::argument_out_of_domain);
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 1);
        lua_rawset(L, -3);
        lua_error(L);
    }

    if (!handle->fiber) {
        push(L, std::errc::invalid_argument);
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 1);
        lua_rawset(L, -3);
        lua_error(L);
    }

    constexpr int extra = /*common path=*/3 +
        /*code branches=*/hana::maximum(hana::tuple_c<int, 1, 2>);
    if (!lua_checkstack(handle->fiber, extra)) {
        vm_ctx.notify_errmem();
        return lua_yield(L, 0);
    }

    rawgetp(handle->fiber, LUA_REGISTRYINDEX, &fiber_list_key);
    lua_pushthread(handle->fiber);
    lua_rawget(handle->fiber, -2);
    lua_rawgeti(handle->fiber, -1, FiberDataIndex::STATUS);
    int status_type = lua_type(handle->fiber, -1);
    if (status_type == LUA_TNIL) {
        // Still running
        lua_pushboolean(handle->fiber, 0);
        lua_rawseti(handle->fiber, -3, FiberDataIndex::JOINER);
        lua_pop(handle->fiber, 3);
    } else { assert(status_type == LUA_TNUMBER);
        // Finished
        if (lua_tointeger(handle->fiber, -1) ==
            FiberStatus::FINISHED_WITH_ERROR) {
            luaL_traceback(L, handle->fiber, nullptr, 1);
            lua_pushvalue(handle->fiber, -4);
            result<std::string, std::bad_alloc> err_str =
                errobj_to_string(handle->fiber);
            lua_pop(handle->fiber, 1);
            if (!err_str) {
                vm_ctx.notify_errmem();
                return lua_yield(L, 0);
            }
            print_panic(handle->fiber, /*is_main=*/false, err_str.value(),
                        tostringview(L, -1));
        }
        lua_pushthread(handle->fiber);
        lua_pushnil(handle->fiber);
        lua_rawset(handle->fiber, -5);
        // TODO (?): force a full GC round on `L()` now
    }
    handle->fiber = nullptr;
    return 0;
}

static int fiber_interrupt(lua_State*)
{
    // TODO
    return 0;
}

static int fiber_meta_index(lua_State* L)
{
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("join"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX, &fiber_join_key);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("detach"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, fiber_detach);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("interrupt"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, fiber_interrupt);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("interruption_caught"),
                [](lua_State* L) -> int {
                    // TODO
                    push(L, errc::bad_index);
                    lua_pushliteral(L, "index");
                    lua_pushvalue(L, 2);
                    lua_rawset(L, -3);
                    return lua_error(L);
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("joinable"),
                [](lua_State* L) -> int {
                    auto handle = reinterpret_cast<fiber_handle*>(
                        lua_touserdata(L, 1));
                    assert(handle);
                    lua_pushboolean(L, handle->fiber != nullptr);
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

static int fiber_meta_gc(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);
    auto handle = reinterpret_cast<fiber_handle*>(lua_touserdata(L, 1));
    assert(handle);

    if (!handle->fiber)
        return 0;

    constexpr int extra = /*common path=*/3 +
        /*code branches=*/hana::maximum(hana::tuple_c<int, 1, 2>);
    if (!lua_checkstack(handle->fiber, extra)) {
        vm_ctx.notify_errmem();
        return 0;
    }

    rawgetp(handle->fiber, LUA_REGISTRYINDEX, &fiber_list_key);
    lua_pushthread(handle->fiber);
    lua_rawget(handle->fiber, -2);
    lua_rawgeti(handle->fiber, -1, FiberDataIndex::STATUS);
    int status_type = lua_type(handle->fiber, -1);
    if (status_type == LUA_TNIL) {
        // Still running
        lua_pushboolean(handle->fiber, 0);
        lua_rawseti(handle->fiber, -3, FiberDataIndex::JOINER);
        lua_pop(handle->fiber, 3);
    } else { assert(status_type == LUA_TNUMBER);
        // Finished
        if (lua_tointeger(handle->fiber, -1) ==
            FiberStatus::FINISHED_WITH_ERROR) {
            luaL_traceback(L, handle->fiber, nullptr, 1);
            lua_pushvalue(handle->fiber, -4);
            result<std::string, std::bad_alloc> err_str =
                errobj_to_string(handle->fiber);
            lua_pop(handle->fiber, 1);
            if (!err_str) {
                vm_ctx.notify_errmem();
                return 0;
            }
            print_panic(handle->fiber, /*is_main=*/false, err_str.value(),
                        tostringview(L, -1));
        }
        lua_pushthread(handle->fiber);
        lua_pushnil(handle->fiber);
        lua_rawset(handle->fiber, -5);
    }
    handle->fiber = nullptr;
    return 0;
}

static int spawn(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TFUNCTION);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto new_fiber = lua_newthread(L);
    assert(new_fiber);

    rawgetp(new_fiber, LUA_REGISTRYINDEX, &fiber_list_key);
    lua_pushthread(new_fiber);
    lua_createtable(new_fiber, /*narr=*/2, /*nrec=*/0);
    lua_rawset(new_fiber, -3);
    lua_pop(new_fiber, 1);

    lua_pushvalue(L, 1);
    lua_xmove(L, new_fiber, 1);

    vm_ctx->strand().defer([vm_ctx,new_fiber]() {
        if (!vm_ctx->valid())
            return;

        vm_ctx->fiber_prologue(new_fiber);
        int res = lua_resume(new_fiber, 0);
        vm_ctx->fiber_epilogue(res);
    }, std::allocator<void>{});

    {
        auto buf = reinterpret_cast<fiber_handle*>(
            lua_newuserdata(L, sizeof(fiber_handle))
        );
        rawgetp(L, LUA_REGISTRYINDEX, &fiber_mt_key);
        int res = lua_setmetatable(L, -2);
        assert(res); boost::ignore_unused(res);
        new (buf) fiber_handle{new_fiber};
    }

    return 1;
}

void init_fiber_module(lua_State* L)
{
    lua_pushliteral(L, "spawn");
    lua_pushcfunction(L, spawn);
    lua_rawset(L, LUA_GLOBALSINDEX);

    lua_pushlightuserdata(L, &fiber_mt_key);

    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "fiber");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, fiber_meta_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, fiber_meta_gc);
        lua_rawset(L, -3);
    }

    lua_rawset(L, LUA_REGISTRYINDEX);

    {
        lua_pushlightuserdata(L, &fiber_join_key);
        int res = luaL_loadbuffer(
            L, reinterpret_cast<char*>(fiber_join_bytecode),
            fiber_join_bytecode_size, nullptr);
        assert(res == 0); boost::ignore_unused(res);
        lua_pushcfunction(L, lua_error);
        rawgetp(L, LUA_REGISTRYINDEX, &raw_unpack_key);
        lua_pushcfunction(L, fiber_join);
        lua_call(L, 3, 1);
        lua_rawset(L, LUA_REGISTRYINDEX);
    }
}

void print_panic(const lua_State* fiber, bool is_main, std::string_view error,
                 std::string_view stacktrace)
{
    constexpr auto spec{FMT_STRING(
        "{}{} {:p} panicked: '{}{}{}'{}\n"
        "{}{}{}\n"
    )};

    std::string_view red{"\033[31;1m"};
    std::string_view dim{"\033[2m"};
    std::string_view underline{"\033[4m"};
    std::string_view reset_red{"\033[22;39m"};
    std::string_view reset_dim{"\033[22m"};
    std::string_view reset_underline{"\033[24m"};
    if (!stdout_has_color)
        red = dim = underline = reset_red = reset_dim = reset_underline = {};

    fmt::print(
        stderr,
        spec,
        red,
        (is_main ? "Main fiber from VM"sv : "Fiber"sv),
        static_cast<const void*>(fiber),
        underline, error, reset_underline,
        reset_red,
        dim, stacktrace, reset_dim);
}

} // namespace emilua
