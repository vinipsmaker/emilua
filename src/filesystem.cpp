/* Copyright (c) 2023 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#include <emilua/dispatch_table.hpp>
#include <emilua/filesystem.hpp>
#include <emilua/windows.hpp>

namespace emilua {

namespace fs = std::filesystem;

char filesystem_key;
char filesystem_path_mt_key;
static char filesystem_path_iterator_mt_key;

static int path_to_generic(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    try {
        auto ret = path->generic_u8string();
        lua_pushlstring(L, reinterpret_cast<char*>(ret.data()), ret.size());
        return 1;
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }
}

static int path_iterator(lua_State* L)
{
    static constexpr auto iter = [](lua_State* L) {
        auto path = static_cast<fs::path*>(
            lua_touserdata(L, lua_upvalueindex(1)));
        auto iter = static_cast<fs::path::iterator*>(
            lua_touserdata(L, lua_upvalueindex(2)));

        if (*iter == path->end())
            return 0;

        auto ret = static_cast<fs::path*>(lua_newuserdata(L, sizeof(fs::path)));
        rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
        setmetatable(L, -2);
        new (ret) fs::path{};
        *ret = **iter;
        ++*iter;
        return 1;
    };

    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    lua_pushvalue(L, 1);
    {
        auto iter = static_cast<fs::path::iterator*>(
            lua_newuserdata(L, sizeof(fs::path::iterator)));
        rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_iterator_mt_key);
        setmetatable(L, -2);
        new (iter) fs::path::iterator{};
        try {
            *iter = path->begin();
        } catch (const std::system_error& e) {
            push(L, e.code());
            return lua_error(L);
        } catch (const std::exception& e) {
            lua_pushstring(L, e.what());
            return lua_error(L);
        }
    }
    lua_pushcclosure(L, iter, 2);
    return 1;
}

static int path_make_preferred(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto ret = static_cast<fs::path*>(lua_newuserdata(L, sizeof(fs::path)));
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    setmetatable(L, -2);
    new (ret) fs::path{*path};
    ret->make_preferred();

    return 1;
}

static int path_remove_filename(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto ret = static_cast<fs::path*>(lua_newuserdata(L, sizeof(fs::path)));
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    setmetatable(L, -2);
    new (ret) fs::path{*path};
    ret->remove_filename();

    return 1;
}

static int path_replace_filename(lua_State* L)
{
    lua_settop(L, 2);

    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    fs::path path2;

    switch (lua_type(L, 2)) {
    case LUA_TSTRING:
        try {
            path2 = fs::path{
                widen_on_windows(tostringview(L, 2)), fs::path::native_format};
            break;
        } catch (const std::system_error& e) {
            push(L, e.code());
            return lua_error(L);
        } catch (const std::exception& e) {
            lua_pushstring(L, e.what());
            return lua_error(L);
        }
    case LUA_TUSERDATA: {
        auto p = static_cast<fs::path*>(lua_touserdata(L, 2));
        if (!p || !lua_getmetatable(L, 2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        path2 = *p;
        break;
    }
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    auto ret = static_cast<fs::path*>(lua_newuserdata(L, sizeof(fs::path)));
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    setmetatable(L, -2);
    new (ret) fs::path{*path};
    ret->replace_filename(path2);

    return 1;
}

static int path_replace_extension(lua_State* L)
{
    lua_settop(L, 2);

    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    fs::path path2;

    switch (lua_type(L, 2)) {
    case LUA_TNIL:
        break;
    case LUA_TSTRING:
        try {
            path2 = fs::path{
                widen_on_windows(tostringview(L, 2)), fs::path::native_format};
            break;
        } catch (const std::system_error& e) {
            push(L, e.code());
            return lua_error(L);
        } catch (const std::exception& e) {
            lua_pushstring(L, e.what());
            return lua_error(L);
        }
    case LUA_TUSERDATA: {
        auto p = static_cast<fs::path*>(lua_touserdata(L, 2));
        if (!p || !lua_getmetatable(L, 2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        path2 = *p;
        break;
    }
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    auto ret = static_cast<fs::path*>(lua_newuserdata(L, sizeof(fs::path)));
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    setmetatable(L, -2);
    new (ret) fs::path{*path};
    ret->replace_extension(path2);

    return 1;
}

static int path_lexically_normal(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto ret = static_cast<fs::path*>(lua_newuserdata(L, sizeof(fs::path)));
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    setmetatable(L, -2);
    new (ret) fs::path{};

    try {
        *ret = path->lexically_normal();
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }

    return 1;
}

static int path_lexically_relative(lua_State* L)
{
    lua_settop(L, 2);

    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    fs::path path2;

    switch (lua_type(L, 2)) {
    case LUA_TSTRING:
        try {
            path2 = fs::path{
                widen_on_windows(tostringview(L, 2)), fs::path::native_format};
            break;
        } catch (const std::system_error& e) {
            push(L, e.code());
            return lua_error(L);
        } catch (const std::exception& e) {
            lua_pushstring(L, e.what());
            return lua_error(L);
        }
    case LUA_TUSERDATA: {
        auto p = static_cast<fs::path*>(lua_touserdata(L, 2));
        if (!p || !lua_getmetatable(L, 2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        path2 = *p;
        break;
    }
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    auto ret = static_cast<fs::path*>(lua_newuserdata(L, sizeof(fs::path)));
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    setmetatable(L, -2);
    new (ret) fs::path{};

    try {
        *ret = path->lexically_relative(path2);
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }

    return 1;
}

static int path_lexically_proximate(lua_State* L)
{
    lua_settop(L, 2);

    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    fs::path path2;

    switch (lua_type(L, 2)) {
    case LUA_TSTRING:
        try {
            path2 = fs::path{
                widen_on_windows(tostringview(L, 2)), fs::path::native_format};
            break;
        } catch (const std::system_error& e) {
            push(L, e.code());
            return lua_error(L);
        } catch (const std::exception& e) {
            lua_pushstring(L, e.what());
            return lua_error(L);
        }
    case LUA_TUSERDATA: {
        auto p = static_cast<fs::path*>(lua_touserdata(L, 2));
        if (!p || !lua_getmetatable(L, 2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        path2 = *p;
        break;
    }
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    auto ret = static_cast<fs::path*>(lua_newuserdata(L, sizeof(fs::path)));
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    setmetatable(L, -2);
    new (ret) fs::path{};

    try {
        *ret = path->lexically_proximate(path2);
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }

    return 1;
}

static int path_root_name(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto ret = static_cast<fs::path*>(lua_newuserdata(L, sizeof(fs::path)));
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    setmetatable(L, -2);
    new (ret) fs::path{};

    try {
        *ret = path->root_name();
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }

    return 1;
}

static int path_root_directory(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto ret = static_cast<fs::path*>(lua_newuserdata(L, sizeof(fs::path)));
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    setmetatable(L, -2);
    new (ret) fs::path{};

    try {
        *ret = path->root_directory();
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }

    return 1;
}

static int path_root_path(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto ret = static_cast<fs::path*>(lua_newuserdata(L, sizeof(fs::path)));
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    setmetatable(L, -2);
    new (ret) fs::path{};

    try {
        *ret = path->root_path();
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }

    return 1;
}

static int path_relative_path(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto ret = static_cast<fs::path*>(lua_newuserdata(L, sizeof(fs::path)));
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    setmetatable(L, -2);
    new (ret) fs::path{};

    try {
        *ret = path->relative_path();
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }

    return 1;
}

static int path_parent_path(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto ret = static_cast<fs::path*>(lua_newuserdata(L, sizeof(fs::path)));
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    setmetatable(L, -2);
    new (ret) fs::path{};

    try {
        *ret = path->parent_path();
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }

    return 1;
}

static int path_filename(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto ret = static_cast<fs::path*>(lua_newuserdata(L, sizeof(fs::path)));
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    setmetatable(L, -2);
    new (ret) fs::path{};

    try {
        *ret = path->filename();
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }

    return 1;
}

static int path_stem(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto ret = static_cast<fs::path*>(lua_newuserdata(L, sizeof(fs::path)));
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    setmetatable(L, -2);
    new (ret) fs::path{};

    try {
        *ret = path->stem();
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }

    return 1;
}

static int path_extension(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto ret = static_cast<fs::path*>(lua_newuserdata(L, sizeof(fs::path)));
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    setmetatable(L, -2);
    new (ret) fs::path{};

    try {
        *ret = path->extension();
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }

    return 1;
}

inline int path_empty(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    lua_pushboolean(L, path->empty());
    return 1;
}

inline int path_has_root_path(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    try {
        lua_pushboolean(L, path->has_root_path());
        return 1;
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }
}

inline int path_has_root_name(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    try {
        lua_pushboolean(L, path->has_root_name());
        return 1;
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }
}

inline int path_has_root_directory(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    try {
        lua_pushboolean(L, path->has_root_directory());
        return 1;
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }
}

inline int path_has_relative_path(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    try {
        lua_pushboolean(L, path->has_relative_path());
        return 1;
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }
}

inline int path_has_parent_path(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    try {
        lua_pushboolean(L, path->has_parent_path());
        return 1;
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }
}

inline int path_has_filename(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    try {
        lua_pushboolean(L, path->has_filename());
        return 1;
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }
}

inline int path_has_stem(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    try {
        lua_pushboolean(L, path->has_stem());
        return 1;
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }
}

inline int path_has_extension(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    try {
        lua_pushboolean(L, path->has_extension());
        return 1;
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }
}

inline int path_is_absolute(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    try {
        lua_pushboolean(L, path->is_absolute());
        return 1;
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }
}

inline int path_is_relative(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    try {
        lua_pushboolean(L, path->is_relative());
        return 1;
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }
}

static int path_mt_index(lua_State* L)
{
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("to_generic"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, path_to_generic);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("iterator"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, path_iterator);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("make_preferred"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, path_make_preferred);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("remove_filename"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, path_remove_filename);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("replace_filename"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, path_replace_filename);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("replace_extension"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, path_replace_extension);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("lexically_normal"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, path_lexically_normal);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("lexically_relative"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, path_lexically_relative);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("lexically_proximate"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, path_lexically_proximate);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("root_name"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, path_root_name);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("root_directory"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, path_root_directory);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("root_path"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, path_root_path);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("relative_path"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, path_relative_path);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("parent_path"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, path_parent_path);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("filename"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, path_filename);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("stem"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, path_stem);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("extension"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, path_extension);
                    return 1;
                }
            ),
            hana::make_pair(BOOST_HANA_STRING("empty"), path_empty),
            hana::make_pair(
                BOOST_HANA_STRING("has_root_path"), path_has_root_path),
            hana::make_pair(
                BOOST_HANA_STRING("has_root_name"), path_has_root_name),
            hana::make_pair(
                BOOST_HANA_STRING("has_root_directory"),
                path_has_root_directory),
            hana::make_pair(
                BOOST_HANA_STRING("has_relative_path"), path_has_relative_path),
            hana::make_pair(
                BOOST_HANA_STRING("has_parent_path"), path_has_parent_path),
            hana::make_pair(
                BOOST_HANA_STRING("has_filename"), path_has_filename),
            hana::make_pair(BOOST_HANA_STRING("has_stem"), path_has_stem),
            hana::make_pair(
                BOOST_HANA_STRING("has_extension"), path_has_extension),
            hana::make_pair(BOOST_HANA_STRING("is_absolute"), path_is_absolute),
            hana::make_pair(BOOST_HANA_STRING("is_relative"), path_is_relative)
        ),
        [](std::string_view /*key*/, lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        },
        tostringview(L, 2),
        L
    );
}

static int path_mt_tostring(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));

    try {
        auto ret = path->u8string();
        lua_pushlstring(L, reinterpret_cast<char*>(ret.data()), ret.size());
        return 1;
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }
}

static int path_mt_eq(lua_State* L)
{
    auto path1 = static_cast<fs::path*>(lua_touserdata(L, 1));
    auto path2 = static_cast<fs::path*>(lua_touserdata(L, 2));
    lua_pushboolean(L, *path1 == *path2);
    return 1;
}

static int path_mt_lt(lua_State* L)
{
    fs::path path1, path2;

    switch (lua_type(L, 1)) {
    case LUA_TSTRING:
        try {
            path1 = fs::path{
                widen_on_windows(tostringview(L, 1)), fs::path::native_format};
            break;
        } catch (const std::system_error& e) {
            push(L, e.code());
            return lua_error(L);
        } catch (const std::exception& e) {
            lua_pushstring(L, e.what());
            return lua_error(L);
        }
    case LUA_TUSERDATA: {
        auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
        if (!path || !lua_getmetatable(L, 1)) {
            push(L, std::errc::invalid_argument, "arg", 1);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 1);
            return lua_error(L);
        }

        path1 = *path;
        break;
    }
    default:
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    switch (lua_type(L, 2)) {
    case LUA_TSTRING:
        try {
            path2 = fs::path{
                widen_on_windows(tostringview(L, 2)), fs::path::native_format};
            break;
        } catch (const std::system_error& e) {
            push(L, e.code());
            return lua_error(L);
        } catch (const std::exception& e) {
            lua_pushstring(L, e.what());
            return lua_error(L);
        }
    case LUA_TUSERDATA: {
        auto path = static_cast<fs::path*>(lua_touserdata(L, 2));
        if (!path || !lua_getmetatable(L, 2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        path2 = *path;
        break;
    }
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    lua_pushboolean(L, path1 < path2);
    return 1;
}

static int path_mt_le(lua_State* L)
{
    fs::path path1, path2;

    switch (lua_type(L, 1)) {
    case LUA_TSTRING:
        try {
            path1 = fs::path{
                widen_on_windows(tostringview(L, 1)), fs::path::native_format};
            break;
        } catch (const std::system_error& e) {
            push(L, e.code());
            return lua_error(L);
        } catch (const std::exception& e) {
            lua_pushstring(L, e.what());
            return lua_error(L);
        }
    case LUA_TUSERDATA: {
        auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
        if (!path || !lua_getmetatable(L, 1)) {
            push(L, std::errc::invalid_argument, "arg", 1);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 1);
            return lua_error(L);
        }

        path1 = *path;
        break;
    }
    default:
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    switch (lua_type(L, 2)) {
    case LUA_TSTRING:
        try {
            path2 = fs::path{
                widen_on_windows(tostringview(L, 2)), fs::path::native_format};
            break;
        } catch (const std::system_error& e) {
            push(L, e.code());
            return lua_error(L);
        } catch (const std::exception& e) {
            lua_pushstring(L, e.what());
            return lua_error(L);
        }
    case LUA_TUSERDATA: {
        auto path = static_cast<fs::path*>(lua_touserdata(L, 2));
        if (!path || !lua_getmetatable(L, 2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        path2 = *path;
        break;
    }
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    lua_pushboolean(L, path1 <= path2);
    return 1;
}

static int path_mt_div(lua_State* L)
{
    lua_settop(L, 2);

    fs::path path1, path2;

    switch (lua_type(L, 1)) {
    case LUA_TSTRING:
        try {
            path1 = fs::path{
                widen_on_windows(tostringview(L, 1)), fs::path::native_format};
            break;
        } catch (const std::system_error& e) {
            push(L, e.code());
            return lua_error(L);
        } catch (const std::exception& e) {
            lua_pushstring(L, e.what());
            return lua_error(L);
        }
    case LUA_TUSERDATA: {
        auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
        if (!path || !lua_getmetatable(L, 1)) {
            push(L, std::errc::invalid_argument, "arg", 1);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 1);
            return lua_error(L);
        }

        path1 = *path;
        break;
    }
    default:
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    switch (lua_type(L, 2)) {
    case LUA_TSTRING:
        try {
            path2 = fs::path{
                widen_on_windows(tostringview(L, 2)), fs::path::native_format};
            break;
        } catch (const std::system_error& e) {
            push(L, e.code());
            return lua_error(L);
        } catch (const std::exception& e) {
            lua_pushstring(L, e.what());
            return lua_error(L);
        }
    case LUA_TUSERDATA: {
        auto path = static_cast<fs::path*>(lua_touserdata(L, 2));
        if (!path || !lua_getmetatable(L, 2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        path2 = *path;
        break;
    }
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    auto path = static_cast<fs::path*>(lua_newuserdata(L, sizeof(fs::path)));
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    setmetatable(L, -2);
    new (path) fs::path{};
    *path = path1 / path2;

    return 1;
}

static int path_mt_concat(lua_State* L)
{
    lua_settop(L, 2);

    fs::path path1, path2;

    switch (lua_type(L, 1)) {
    case LUA_TSTRING:
        try {
            path1 = fs::path{
                widen_on_windows(tostringview(L, 1)), fs::path::native_format};
            break;
        } catch (const std::system_error& e) {
            push(L, e.code());
            return lua_error(L);
        } catch (const std::exception& e) {
            lua_pushstring(L, e.what());
            return lua_error(L);
        }
    case LUA_TUSERDATA: {
        auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
        if (!path || !lua_getmetatable(L, 1)) {
            push(L, std::errc::invalid_argument, "arg", 1);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 1);
            return lua_error(L);
        }

        path1 = *path;
        break;
    }
    default:
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    switch (lua_type(L, 2)) {
    case LUA_TSTRING:
        try {
            path2 = fs::path{
                widen_on_windows(tostringview(L, 2)), fs::path::native_format};
            break;
        } catch (const std::system_error& e) {
            push(L, e.code());
            return lua_error(L);
        } catch (const std::exception& e) {
            lua_pushstring(L, e.what());
            return lua_error(L);
        }
    case LUA_TUSERDATA: {
        auto path = static_cast<fs::path*>(lua_touserdata(L, 2));
        if (!path || !lua_getmetatable(L, 2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        path2 = *path;
        break;
    }
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    auto path = static_cast<fs::path*>(lua_newuserdata(L, sizeof(fs::path)));
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    setmetatable(L, -2);
    new (path) fs::path{path1};
    *path += path2;

    return 1;
}

static int path_new(lua_State* L)
{
    lua_settop(L, 1);

    auto path = static_cast<fs::path*>(lua_newuserdata(L, sizeof(fs::path)));
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    setmetatable(L, -2);
    new (path) fs::path{};

    switch (lua_type(L, 1)) {
    case LUA_TNIL:
        break;
    case LUA_TSTRING:
        try {
            *path = fs::path{
                widen_on_windows(tostringview(L, 1)), fs::path::native_format};
            break;
        } catch (const std::system_error& e) {
            push(L, e.code());
            return lua_error(L);
        } catch (const std::exception& e) {
            lua_pushstring(L, e.what());
            return lua_error(L);
        }
    default:
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    return 1;
}

static int path_from_generic(lua_State* L)
{
    if (lua_type(L, 1) != LUA_TSTRING) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto path = static_cast<fs::path*>(lua_newuserdata(L, sizeof(fs::path)));
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    setmetatable(L, -2);
    new (path) fs::path{};

    try {
        *path = fs::path{
            widen_on_windows(tostringview(L, 1)), fs::path::generic_format};
        return 1;
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }
}

void init_filesystem(lua_State* L)
{
    lua_pushlightuserdata(L, &filesystem_path_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/9);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "filesystem.path");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, path_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__tostring");
        lua_pushcfunction(L, path_mt_tostring);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__eq");
        lua_pushcfunction(L, path_mt_eq);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__lt");
        lua_pushcfunction(L, path_mt_lt);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__le");
        lua_pushcfunction(L, path_mt_le);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__div");
        lua_pushcfunction(L, path_mt_div);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__concat");
        lua_pushcfunction(L, path_mt_concat);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<fs::path>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &filesystem_path_iterator_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/1);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<fs::path::iterator>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &filesystem_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/1);

        lua_pushliteral(L, "path");
        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/3);

            lua_pushliteral(L, "new");
            lua_pushcfunction(L, path_new);
            lua_rawset(L, -3);

            lua_pushliteral(L, "from_generic");
            lua_pushcfunction(L, path_from_generic);
            lua_rawset(L, -3);

            lua_pushliteral(L, "preferred_separator");
            {
                fs::path::value_type sep = fs::path::preferred_separator;
                push(L, narrow_on_windows(&sep, 1));
            }
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);
}

} // namespace emilua
