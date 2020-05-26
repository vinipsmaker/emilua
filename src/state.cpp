#include <iostream>
#include <fstream>
#include <new>

#include <emilua/detail/core.hpp>
#include <emilua/fiber.hpp>
#include <emilua/state.hpp>
#include <emilua/timer.hpp>

namespace emilua {

using namespace std::string_view_literals;
namespace fs = std::filesystem;

static int println(lua_State* L)
{
    auto msg = luaL_checkstring(L, 1);
    std::cout << msg << std::endl;
    return 0;
}

static int require(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);
    luaL_checktype(L, 1, LUA_TSTRING);
    auto module = tostringview(L, 1);
    if (module == "sleep_for") {
        result<void, std::bad_alloc> res = push_sleep_for(L);
        if (!res) {
            vm_ctx.notify_errmem();
            return lua_yield(L, 0);
        }
        return 1;
    } else if (module == "println") {
        lua_pushcfunction(L, println);
        return 1;
    } else {
        push(L, errc::module_not_found);
        return lua_error(L);
    }
    return 0;
}

std::shared_ptr<vm_context> make_vm(asio::io_context& ioctx, int& exit_code,
                                    fs::path entry_point,
                                    ContextType lua_context)
{
    switch (lua_context) {
    case ContextType::regular_context:
    case ContextType::error_category:
        throw exception{errc::bad_root_context};
    case ContextType::main:
    case ContextType::test:
    case ContextType::worker:
        break;
    }

    if (!entry_point.is_absolute())
        entry_point = std::filesystem::absolute(std::move(entry_point));

    asio::io_context::strand strand{ioctx};
    auto state = std::make_shared<vm_context>(strand);
    assert(state->valid());

    lua_State* L = state->L();

    {
        lua_pushlightuserdata(L, &detail::context_key);
        lua_pushlightuserdata(L, state.get());
        lua_rawset(L, LUA_REGISTRYINDEX);
    }

    {
        lua_pushlightuserdata(L, &detail::error_category_key);
        lua_createtable(L, /*narr=*/0, /*nrec=*/4);

        lua_pushliteral(L, "__metatable");
        lua_pushlightuserdata(L, nullptr);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__eq");
        lua_pushcfunction(
            L,
            [](lua_State* L) -> int {
                auto cat1 = reinterpret_cast<std::error_category**>(
                    lua_touserdata(L, 1));
                assert(cat1);
                auto cat2 = reinterpret_cast<std::error_category**>(
                    lua_touserdata(L, 1));
                assert(cat2);
                return *cat1 == *cat2;
            }
        );
        lua_rawset(L, -3);

        lua_pushliteral(L, "__tostring");
        lua_pushcfunction(
            L,
            [](lua_State* L) -> int {
                auto cat = reinterpret_cast<std::error_category**>(
                    lua_touserdata(L, 1));
                assert(cat);
                lua_pushstring(L, (*cat)->name());
                return 1;
            }
        );
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(
            L,
            [](lua_State* L) -> int {
                auto cat = reinterpret_cast<std::error_category**>(
                    lua_touserdata(L, 1));
                assert(cat);
                auto key = tostringview(L, 2);
                if (key == "message") {
                    lua_pushlightuserdata(L, *cat);
                    lua_pushcclosure(
                        L,
                        [](lua_State* L) -> int {
                            luaL_checktype(L, 1, LUA_TNUMBER);
                            auto cat = reinterpret_cast<std::error_category*>(
                                lua_touserdata(L, lua_upvalueindex(1)));
                            push(L, cat->message(lua_tonumber(L, 1)));
                            return 1;
                        },
                        1
                    );
                    return 1;
                } else {
                    push(L, errc::bad_index);
                    lua_pushliteral(L, "index");
                    lua_pushvalue(L, 2);
                    lua_rawset(L, -3);
                    return lua_error(L);
                }
            }
        );
        lua_rawset(L, -3);

        lua_rawset(L, LUA_REGISTRYINDEX);
    }

    {
        lua_pushlightuserdata(L, &detail::error_code_key);
        lua_createtable(L, /*narr=*/0, /*nrec=*/2);

        lua_pushliteral(L, "__eq");
        lua_pushcfunction(
            L,
            [](lua_State* L) -> int {
                luaL_checktype(L, 1, LUA_TTABLE);
                luaL_checktype(L, 2, LUA_TTABLE);
                lua_pushliteral(L, "val");
                lua_pushvalue(L, -1);
                lua_rawget(L, 1);
                lua_pushvalue(L, -2);
                lua_rawget(L, 2);
                if (!lua_rawequal(L, -1, -2)) {
                    lua_pushboolean(L, 0);
                    return 1;
                }
                lua_pushliteral(L, "cat");
                lua_pushvalue(L, -1);
                lua_rawget(L, 1);
                lua_pushvalue(L, -2);
                lua_rawget(L, 2);
                lua_pushboolean(L, lua_equal(L, -1, -2));
                return 1;
            }
        );
        lua_rawset(L, -3);

        lua_pushliteral(L, "__tostring");
        lua_pushcfunction(
            L,
            [](lua_State* L) -> int {
                luaL_checktype(L, 1, LUA_TTABLE);
                lua_pushliteral(L, "val");
                lua_rawget(L, 1);
                if (lua_type(L, -1) != LUA_TNUMBER) {
                    push(L, std::errc::invalid_argument);
                    return lua_error(L);
                }
                int val = lua_tonumber(L, -1);
                lua_pushliteral(L, "cat");
                lua_rawget(L, 1);
                if (!lua_getmetatable(L, -1)) {
                    push(L, std::errc::invalid_argument);
                    return lua_error(L);
                }
                rawgetp(L, LUA_REGISTRYINDEX, &detail::error_category_key);
                if (!lua_rawequal(L, -1, -2)) {
                    push(L, std::errc::invalid_argument);
                    return lua_error(L);
                }
                auto cat = reinterpret_cast<std::error_category**>(
                    lua_touserdata(L, -3));
                assert(cat);
                push(L, (*cat)->message(val));
                return 1;
            }
        );
        lua_rawset(L, -3);

        lua_rawset(L, LUA_REGISTRYINDEX);
    }

    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/0);

        lua_pushlightuserdata(L, &fiber_list_key);
        lua_pushvalue(L, -2);
        lua_rawset(L, LUA_REGISTRYINDEX);

        lua_pushthread(L);
        lua_createtable(L, /*narr=*/3, /*nrec=*/0);

        lua_createtable(L, /*narr=*/1, /*nrec=*/0);
        push(L, entry_point);
        lua_rawseti(L, -2, 1);
        lua_rawseti(L, -2, FiberDataIndex::STACK);

        lua_pushboolean(L, 0);
        lua_rawseti(L, -2, FiberDataIndex::LEAF);

        lua_pushinteger(L, lua_context);
        lua_rawseti(L, -2, FiberDataIndex::CONTEXT);

        lua_rawset(L, -3);
        lua_pop(L, 1);
    }

    {
        std::string_view ctx_str;
        switch (lua_context) {
        case ContextType::regular_context:
        case ContextType::error_category:
            assert(false);
            break;
        case ContextType::main:
            ctx_str = "main";
            break;
        case ContextType::test:
            ctx_str = "test";
            break;
        case ContextType::worker:
            ctx_str = "worker";
            break;
        }
        push(L, ctx_str);
        lua_setglobal(L, "_CONTEXT");
    }

    lua_pushcfunction(L, require);
    lua_setglobal(L, "require");

    lua_pushcfunction(L, luaopen_base);
    lua_call(L, 0, 0);

    lua_pushlightuserdata(L, &raw_unpack_key);
    lua_pushliteral(L, "unpack");
    lua_rawget(L, LUA_GLOBALSINDEX);
    lua_rawset(L, LUA_REGISTRYINDEX);

    init_fiber_module(L);

    std::optional<std::reference_wrapper<std::string>> module_source;
    {
        auto& service = asio::use_service<emilua::service>(strand.context());
        [[maybe_unused]]
        std::lock_guard guard{service.modules_cache_registry_mtx};
        auto it = service.modules_cache_registry.find(entry_point);
        if (it == service.modules_cache_registry.end()) {
            try {
                std::string contents;
                std::ifstream in{entry_point, std::ios::in | std::ios::binary};
                in.exceptions(std::ios_base::badbit | std::ios_base::failbit |
                              std::ios_base::eofbit);
                in.seekg(0, std::ios::end);
                contents.resize(in.tellg());
                in.seekg(0, std::ios::beg);
                in.read(&contents[0], contents.size());
                in.close();

                // TODO: store bytecode instead source
                it = service.modules_cache_registry
                    .emplace(entry_point, std::move(contents))
                    .first;
            } catch (const std::ios_base::failure&) {
                throw exception{errc::failed_to_load_module};
            }
        }
        module_source = it->second;
    }
    assert(module_source);

    std::string name{'@'};
    name += entry_point;
    switch (int res = luaL_loadbuffer(
        L, module_source->get().data(), module_source->get().size(), name.data()
    ) ; res) {
    case 0:
        break;
    case LUA_ERRMEM:
        throw std::bad_alloc();
    default: {
        std::size_t len;
        const char* str = lua_tolstring(L, -1, &len);
        throw lua_exception{res, std::string{str, len}};
    }
    }

    return state;
}

} // namespace emilua
