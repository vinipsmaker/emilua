#include <emilua/lua_shim.hpp>
#include <emilua/fiber.hpp>

namespace emilua {

extern unsigned char coroutine_create_bytecode[];
extern std::size_t coroutine_create_bytecode_size;
extern unsigned char coroutine_resume_bytecode[];
extern std::size_t coroutine_resume_bytecode_size;
extern unsigned char coroutine_wrap_bytecode[];
extern std::size_t coroutine_wrap_bytecode_size;

static int coroutine_running(lua_State* L)
{
    rawgetp(L, LUA_REGISTRYINDEX, &fiber_list_key);
    lua_pushthread(L);
    lua_rawget(L, -2);
    switch (lua_type(L, -1)) {
    case LUA_TNIL:
        lua_pushthread(L);
        return 1;
    case LUA_TTABLE:
        lua_pushnil(L);
        return 1;
    default:
        assert(false);
        std::abort();
    }
}

static int coroutine_yield(lua_State* L)
{
    rawgetp(L, LUA_REGISTRYINDEX, &fiber_list_key);
    lua_pushthread(L);
    lua_rawget(L, -2);
    switch (lua_type(L, -1)) {
    case LUA_TNIL:
        lua_pop(L, 2);
        lua_pushlightuserdata(L, &yield_reason_is_native_key);
        lua_pushboolean(L, 0);
        lua_rawset(L, LUA_REGISTRYINDEX);
        return lua_yield(L, lua_gettop(L));
    case LUA_TTABLE:
        push(L, errc::bad_coroutine).value();
        return lua_error(L);
    default:
        assert(false);
        std::abort();
    }
}

void init_lua_shim_module(lua_State* L)
{
    lua_pushliteral(L, "coroutine");
    lua_rawget(L, LUA_GLOBALSINDEX);
    {
        lua_pushliteral(L, "running");
        lua_pushcfunction(L, coroutine_running);
        lua_rawset(L, -3);

        lua_pushliteral(L, "create");
        int res = luaL_loadbuffer(
            L, reinterpret_cast<char*>(coroutine_create_bytecode),
            coroutine_create_bytecode_size, nullptr);
        assert(res == 0); boost::ignore_unused(res);
        lua_pushvalue(L, -2);
        lua_rawget(L, -4);
        lua_pushcfunction(
            L,
            [](lua_State* L) -> int {
                lua_pushlightuserdata(L, &yield_reason_is_native_key);
                lua_pushboolean(L, 0);
                lua_rawset(L, LUA_REGISTRYINDEX);
                return 0;
            });
        lua_pushliteral(L, "unpack");
        lua_rawget(L, LUA_GLOBALSINDEX);
        lua_call(L, 3, 1);
        lua_rawset(L, -3);

        lua_pushliteral(L, "resume");
        res = luaL_loadbuffer(
            L, reinterpret_cast<char*>(coroutine_resume_bytecode),
            coroutine_resume_bytecode_size, nullptr);
        assert(res == 0); boost::ignore_unused(res);
        lua_pushvalue(L, -2);
        lua_rawget(L, -4);
        lua_pushliteral(L, "yield");
        lua_rawget(L, -5);
        lua_pushcfunction(
            L,
            [](lua_State* L) -> int {
                rawgetp(L, LUA_REGISTRYINDEX, &yield_reason_is_native_key);
                return 1;
            });
        lua_pushcfunction(
            L,
            [](lua_State* L) -> int {
                lua_pushlightuserdata(L, &yield_reason_is_native_key);
                lua_pushboolean(L, 1);
                lua_rawset(L, LUA_REGISTRYINDEX);
                return 0;
            });
        lua_pushliteral(L, "unpack");
        lua_rawget(L, LUA_GLOBALSINDEX);
        lua_call(L, 5, 1);
        lua_rawset(L, -3);

        lua_pushliteral(L, "wrap");
        res = luaL_loadbuffer(
            L, reinterpret_cast<char*>(coroutine_wrap_bytecode),
            coroutine_wrap_bytecode_size, nullptr);
        assert(res == 0); boost::ignore_unused(res);
        lua_pushliteral(L, "create");
        lua_rawget(L, -4);
        lua_pushliteral(L, "resume");
        lua_rawget(L, -5);
        lua_pushcfunction(L, lua_error);
        lua_pushliteral(L, "unpack");
        lua_rawget(L, LUA_GLOBALSINDEX);
        lua_call(L, 4, 1);
        lua_rawset(L, -3);

        lua_pushliteral(L, "yield");
        lua_pushcfunction(L, coroutine_yield);
        lua_rawset(L, -3);
    }
    lua_pop(L, 1);
}

} // namespace emilua
