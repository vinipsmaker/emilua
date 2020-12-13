#include <cstdio>

#include <emilua/fiber.hpp>
#include <emilua/dispatch_table.hpp>
#include <emilua/scope_cleanup.hpp>

#include <fmt/format.h>

#include <boost/hana/maximum.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/plus.hpp>

namespace emilua {

extern unsigned char fiber_join_bytecode[];
extern std::size_t fiber_join_bytecode_size;
extern unsigned char spawn_start_fn_bytecode[];
extern std::size_t spawn_start_fn_bytecode_size;

char fiber_list_key;
char yield_reason_is_native_key;
static char spawn_start_fn_key;
static char fiber_mt_key;
static char fiber_join_key;

static int fiber_join(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);
    EMILUA_CHECK_SUSPEND_ALLOWED(vm_ctx, L);

    auto handle = reinterpret_cast<fiber_handle*>(lua_touserdata(L, 1));
    if (!handle || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument);
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &fiber_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument);
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }

    if (!handle->fiber || handle->join_in_progress) {
        push(L, std::errc::invalid_argument);
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }

    if (handle->fiber == vm_ctx.current_fiber()) {
        push(L, std::errc::resource_deadlock_would_occur);
        return lua_error(L);
    }

    rawgetp(handle->fiber, LUA_REGISTRYINDEX, &fiber_list_key);
    lua_pushthread(handle->fiber);
    lua_rawget(handle->fiber, -2);
    lua_replace(handle->fiber, -2);
    lua_xmove(handle->fiber, L, 1);

    lua_rawgeti(L, -1, FiberDataIndex::STATUS);
    int status_type = lua_type(L, -1);
    if (status_type == LUA_TNIL) {
        // Still running
        lua_pushthread(vm_ctx.current_fiber());
        lua_xmove(vm_ctx.current_fiber(), L, 1);
        lua_rawseti(L, -3, FiberDataIndex::JOINER);

        lua_pushvalue(L, 1);
        lua_rawseti(L, -3, FiberDataIndex::USER_HANDLE);

        lua_pushvalue(L, 1);
        lua_pushlightuserdata(L, vm_ctx.current_fiber());
        lua_pushthread(handle->fiber);
        lua_xmove(handle->fiber, L, 1);
        lua_pushcclosure(
            L,
            [](lua_State* L) -> int {
                auto vm_ctx = get_vm_context(L).shared_from_this();
                auto handle = reinterpret_cast<fiber_handle*>(
                    lua_touserdata(L, lua_upvalueindex(1)));
                auto current_fiber = reinterpret_cast<lua_State*>(
                    lua_touserdata(L, lua_upvalueindex(2)));
                rawgetp(L, LUA_REGISTRYINDEX, &fiber_list_key);
                lua_pushvalue(L, lua_upvalueindex(3));
                lua_rawget(L, -2);

                lua_pushnil(L);
                lua_rawseti(L, -2, FiberDataIndex::JOINER);

                lua_pushnil(L);
                lua_rawseti(L, -2, FiberDataIndex::USER_HANDLE);

                handle->join_in_progress = false;

                vm_ctx->strand().post(
                    [vm_ctx,current_fiber]() {
                        vm_ctx->fiber_prologue(
                            current_fiber,
                            [&]() {
                                lua_pushboolean(current_fiber, 0);
                                push(current_fiber, errc::interrupted);
                            });
                        int res = lua_resume(current_fiber, 2);
                        vm_ctx->fiber_epilogue(res);
                    },
                    std::allocator<void>{}
                );
                return 0;
            },
            3);
        set_interrupter(L);

        handle->join_in_progress = true;

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
            handle->interruption_caught = false;
            // TODO (?): force a full GC round now
            return nret + 1;
        }
        case FiberStatus::FINISHED_WITH_ERROR: {
            lua_xmove(handle->fiber, L, 1);
            rawgetp(handle->fiber, LUA_REGISTRYINDEX, &fiber_list_key);
            lua_pushthread(handle->fiber);
            lua_pushnil(handle->fiber);
            lua_rawset(handle->fiber, -3);
            handle->fiber = nullptr;
            auto err_obj = inspect_errobj(L);
            if (auto e = std::get_if<std::error_code>(&err_obj) ; e) {
                handle->interruption_caught = *e == errc::interrupted;
            } else {
                handle->interruption_caught = false;
            }
            if (handle->interruption_caught.value()) {
                lua_pushboolean(L, 1);
                return 1;
            } else {
                return lua_error(L);
            }
        }
        default: assert(false);
        }
    }
    return 0;
}

static int fiber_detach(lua_State* L)
{
    auto handle = reinterpret_cast<fiber_handle*>(lua_touserdata(L, 1));
    if (!handle || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument);
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &fiber_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument);
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }

    if (!handle->fiber || handle->join_in_progress) {
        push(L, std::errc::invalid_argument);
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
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
            lua_rawgeti(handle->fiber, -2, FiberDataIndex::STACKTRACE);
            lua_xmove(handle->fiber, L, 1);
            lua_pushvalue(handle->fiber, -4);
            auto err_obj = inspect_errobj(handle->fiber);
            lua_pop(handle->fiber, 1);
            if (auto e = std::get_if<std::error_code>(&err_obj) ;
                !e || *e != errc::interrupted) {
                print_panic(handle->fiber, /*is_main=*/false,
                            errobj_to_string(err_obj), tostringview(L, -1));
            }
        }
        lua_pushthread(handle->fiber);
        lua_pushnil(handle->fiber);
        lua_rawset(handle->fiber, -5);
        // TODO (?): force a full GC round on `L()` now
    }
    handle->fiber = nullptr;
    return 0;
}

static int fiber_interrupt(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);
    auto handle = reinterpret_cast<fiber_handle*>(lua_touserdata(L, 1));
    if (!handle || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument);
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &fiber_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument);
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }

    if (!handle->fiber)
        return 0;

    rawgetp(handle->fiber, LUA_REGISTRYINDEX, &fiber_list_key);
    lua_pushthread(handle->fiber);
    lua_rawget(handle->fiber, -2);
    lua_replace(handle->fiber, -2);
    lua_xmove(handle->fiber, L, 1);

    lua_pushboolean(L, 1);
    lua_rawseti(L, -2, FiberDataIndex::INTERRUPTED);

    if (handle->fiber == vm_ctx.current_fiber())
        return 0;

    lua_rawgeti(L, -1, FiberDataIndex::INTERRUPTER);
    if (lua_type(L, -1) != LUA_TNIL) {
        lua_call(L, 0, 0);

        lua_pushnil(L);
        lua_rawseti(L, -2, FiberDataIndex::INTERRUPTER);
    }

    return 0;
}

inline int fiber_interruption_caught(lua_State* L)
{
    auto handle = reinterpret_cast<fiber_handle*>(lua_touserdata(L, 1));
    assert(handle);
    if (!handle->interruption_caught) {
        push(L, std::errc::invalid_argument);
        return lua_error(L);
    }
    lua_pushboolean(L, handle->interruption_caught.value());
    return 1;
}

inline int fiber_joinable(lua_State* L)
{
    auto handle = reinterpret_cast<fiber_handle*>(lua_touserdata(L, 1));
    assert(handle);
    lua_pushboolean(L, handle->fiber != nullptr && !handle->join_in_progress);
    return 1;
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
                fiber_interruption_caught
            ),
            hana::make_pair(BOOST_HANA_STRING("joinable"), fiber_joinable)
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
    assert(!handle->join_in_progress);

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
            lua_rawgeti(handle->fiber, -2, FiberDataIndex::STACKTRACE);
            lua_xmove(handle->fiber, L, 1);
            lua_pushvalue(handle->fiber, -4);
            auto err_obj = inspect_errobj(handle->fiber);
            lua_pop(handle->fiber, 1);
            if (auto e = std::get_if<std::error_code>(&err_obj) ;
                !e || *e != errc::interrupted) {
                print_panic(handle->fiber, /*is_main=*/false,
                            errobj_to_string(err_obj), tostringview(L, -1));
            }
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
    init_new_coro_or_fiber_scope(new_fiber, L);

    rawgetp(new_fiber, LUA_REGISTRYINDEX, &fiber_list_key);
    lua_pushthread(new_fiber);
    lua_createtable(
        new_fiber,
        /*narr=*/EMILUA_IMPL_INITIAL_FIBER_DATA_CAPACITY,
        /*nrec=*/0);
    {
        lua_pushinteger(new_fiber, 0);
        lua_rawseti(new_fiber, -2, FiberDataIndex::INTERRUPTION_DISABLED);
    }
    {
        rawgetp(L, LUA_REGISTRYINDEX, &fiber_list_key);
        lua_pushthread(vm_ctx->current_fiber());
        lua_xmove(vm_ctx->current_fiber(), L, 1);
        lua_rawget(L, -2);
        lua_rawgeti(L, -1, FiberDataIndex::SOURCE_PATH);
        lua_xmove(L, new_fiber, 1);
        lua_rawseti(new_fiber, -2, FiberDataIndex::SOURCE_PATH);
    }
    lua_rawset(new_fiber, -3);
    lua_pop(new_fiber, 1);

    rawgetp(L, LUA_REGISTRYINDEX, &spawn_start_fn_key);
    lua_pushvalue(L, 1);
    lua_call(L, 1, 1);
    lua_xmove(L, new_fiber, 1);

    vm_ctx->strand().post([vm_ctx,new_fiber]() {
        vm_ctx->fiber_resume_trivial(new_fiber);
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

static int this_fiber_yield(lua_State* L)
{
    auto vm_ctx = get_vm_context(L).shared_from_this();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto current_fiber = vm_ctx->current_fiber();
    vm_ctx->strand().defer(
        [vm_ctx,current_fiber]() {
            vm_ctx->fiber_resume_trivial(current_fiber);
        },
        std::allocator<void>{}
    );
    return lua_yield(L, 0);
}

inline int increment_this_fiber_counter(lua_State* L, FiberDataIndex counter)
{
    auto current_fiber = get_vm_context(L).current_fiber();

    rawgetp(L, LUA_REGISTRYINDEX, &fiber_list_key);
    lua_pushthread(current_fiber);
    lua_xmove(current_fiber, L, 1);
    lua_rawget(L, -2);
    lua_rawgeti(L, -1, counter);
    auto count = lua_tointeger(L, -1);
    ++count;
    assert(count >= 0); //< TODO: better overflow detection and VM shutdown
    lua_pushinteger(L, count);
    lua_rawseti(L, -3, counter);
    return 0;
}

inline int decrement_this_fiber_counter(lua_State* L, FiberDataIndex counter,
                                        errc e)
{
    auto current_fiber = get_vm_context(L).current_fiber();

    rawgetp(L, LUA_REGISTRYINDEX, &fiber_list_key);
    lua_pushthread(current_fiber);
    lua_xmove(current_fiber, L, 1);
    lua_rawget(L, -2);
    lua_rawgeti(L, -1, counter);
    auto count = lua_tointeger(L, -1);
    if (!(count > 0)) {
        push(L, e);
        return lua_error(L);
    }
    lua_pushinteger(L, --count);
    lua_rawseti(L, -3, counter);
    return 0;
}

static int this_fiber_disable_interruption(lua_State* L)
{
    return increment_this_fiber_counter(
        L, FiberDataIndex::INTERRUPTION_DISABLED);
}

static int this_fiber_restore_interruption(lua_State* L)
{
    return decrement_this_fiber_counter(
        L, FiberDataIndex::INTERRUPTION_DISABLED,
        errc::interruption_already_allowed);
}

static int this_fiber_forbid_suspend(lua_State* L)
{
    return increment_this_fiber_counter(
        L, FiberDataIndex::SUSPENSION_DISALLOWED);
}

static int this_fiber_allow_suspend(lua_State* L)
{
    return decrement_this_fiber_counter(
        L, FiberDataIndex::SUSPENSION_DISALLOWED,
        errc::suspension_already_allowed);
}

inline int this_fiber_local(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);

    rawgetp(L, LUA_REGISTRYINDEX, &fiber_list_key);
    lua_pushthread(vm_ctx.current_fiber());
    lua_xmove(vm_ctx.current_fiber(), L, 1);
    lua_rawget(L, -2);
    lua_rawgeti(L, -1, FiberDataIndex::LOCAL_STORAGE);
    if (lua_type(L, -1) == LUA_TNIL) {
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_rawseti(L, -4, FiberDataIndex::LOCAL_STORAGE);
    }
    return 1;
}

inline int this_fiber_is_main(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);

    rawgetp(L, LUA_REGISTRYINDEX, &fiber_list_key);
    lua_pushthread(vm_ctx.current_fiber());
    lua_xmove(vm_ctx.current_fiber(), L, 1);
    lua_rawget(L, -2);
    lua_rawgeti(L, -1, FiberDataIndex::LEAF);
    int has_leaf;
    switch (lua_type(L, -1)) {
    case LUA_TNIL:
        has_leaf = false;
        break;
    case LUA_TBOOLEAN:
        has_leaf = true;
        break;
    default:
        assert(false);
    }
    lua_pushboolean(L, has_leaf);
    return 1;
}

inline int this_fiber_id(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);
    void* id;

    rawgetp(L, LUA_REGISTRYINDEX, &fiber_list_key);
    lua_pushthread(vm_ctx.current_fiber());
    lua_xmove(vm_ctx.current_fiber(), L, 1);
    lua_rawget(L, -2);
    lua_rawgeti(L, -1, FiberDataIndex::LEAF);
    int has_leaf; //< i.e. it is a module fiber
    switch (lua_type(L, -1)) {
    case LUA_TNIL:
        has_leaf = false;
        break;
    case LUA_TBOOLEAN:
        has_leaf = true;
        break;
    default:
        assert(false);
    }
    id = (has_leaf ? vm_ctx.L() : vm_ctx.current_fiber());

    lua_pushfstring(L, "%p", id);
    return 1;
}

static int this_fiber_meta_index(lua_State* L)
{
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("yield"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, this_fiber_yield);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("restore_interruption"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, this_fiber_restore_interruption);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("disable_interruption"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, this_fiber_disable_interruption);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("allow_suspend"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, this_fiber_allow_suspend);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("forbid_suspend"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, this_fiber_forbid_suspend);
                    return 1;
                }
            ),
            hana::make_pair(BOOST_HANA_STRING("local_"), this_fiber_local),
            hana::make_pair(BOOST_HANA_STRING("is_main"), this_fiber_is_main),
            hana::make_pair(BOOST_HANA_STRING("id"), this_fiber_id)
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
        lua_pushlightuserdata(L, &spawn_start_fn_key);
        int res = luaL_loadbuffer(
            L, reinterpret_cast<char*>(spawn_start_fn_bytecode),
            spawn_start_fn_bytecode_size, nullptr);
        assert(res == 0); boost::ignore_unused(res);
        lua_pushcfunction(L, root_scope);
        lua_pushcfunction(L, set_current_traceback);
        lua_pushcfunction(L, terminate_vm_with_cleanup_error);
        rawgetp(L, LUA_REGISTRYINDEX, &raw_xpcall_key);
        rawgetp(L, LUA_REGISTRYINDEX, &raw_pcall_key);
        lua_pushcfunction(L, lua_error);
        rawgetp(L, LUA_REGISTRYINDEX, &raw_unpack_key);
        lua_call(L, 7, 1);
        lua_rawset(L, LUA_REGISTRYINDEX);
    }

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

    lua_pushliteral(L, "this_fiber");
    lua_newtable(L);
    {
        lua_newtable(L);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "this_fiber");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, this_fiber_meta_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__newindex");
        lua_pushcfunction(
            L,
            [](lua_State* L) -> int {
                push(L, std::errc::operation_not_permitted);
                return lua_error(L);
            });
        lua_rawset(L, -3);
    }
    lua_setmetatable(L, -2);
    lua_rawset(L, LUA_GLOBALSINDEX);
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

int set_current_traceback(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);
    rawgetp(L, LUA_REGISTRYINDEX, &fiber_list_key);
    lua_pushthread(vm_ctx.current_fiber());
    lua_xmove(vm_ctx.current_fiber(), L, 1);
    lua_rawget(L, -2);
    luaL_traceback(L, L, nullptr, 3);
    lua_rawseti(L, -2, FiberDataIndex::STACKTRACE);
    return 0;
}

} // namespace emilua
