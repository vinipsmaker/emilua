#include <emilua/actor.hpp>
#include <emilua/state.hpp>
#include <emilua/fiber.hpp>

namespace emilua {

// TODO: fix it
static int dummy;

static int spawn_vm(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TSTRING);
    auto& vm_ctx = get_vm_context(L);
    auto module = tostringview(L, 1);
    if (module == ".") {
        rawgetp(L, LUA_REGISTRYINDEX, &fiber_list_key);
        lua_pushthread(vm_ctx.current_fiber());
        lua_xmove(vm_ctx.current_fiber(), L, 1);
        lua_rawget(L, -2);
        lua_rawgeti(L, -1, FiberDataIndex::SOURCE_PATH);
        module = tostringview(L, -1);
    }

    try {
        auto new_vm_ctx = emilua::make_vm(vm_ctx.strand().context(), dummy,
                                          module, emilua::ContextType::worker);
        new_vm_ctx->strand().post([new_vm_ctx]() {
            new_vm_ctx->fiber_resume_trivial(new_vm_ctx->L());
        }, std::allocator<void>{});
    } catch (const std::bad_alloc&) {
        push(L, std::errc::not_enough_memory);
        return lua_error(L);
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }

    return 0;
}

void init_actor_module(lua_State* L)
{
    lua_pushliteral(L, "spawn_vm");
    lua_pushcfunction(L, spawn_vm);
    lua_rawset(L, LUA_GLOBALSINDEX);
}

} // namespace emilua
