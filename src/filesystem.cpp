/* Copyright (c) 2023 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#include <emilua/dispatch_table.hpp>
#include <emilua/filesystem.hpp>
#include <emilua/windows.hpp>
#include <emilua/time.hpp>

namespace emilua {

namespace fs = std::filesystem;

char filesystem_key;
char filesystem_path_mt_key;
static char filesystem_path_iterator_mt_key;
static char file_clock_time_point_mt_key;
static char space_info_mt_key;
static char file_status_mt_key;
static char directory_entry_mt_key;
static char directory_iterator_mt_key;
static char recursive_directory_iterator_mt_key;

using lua_Seconds = std::chrono::duration<lua_Number>;

struct directory_iterator
{
    template<class... Args>
    directory_iterator(Args&&... args)
        : iterator{std::forward<Args>(args)...}
    {}

    fs::directory_iterator iterator;
    bool increment = false;

    static int next(lua_State* L);
    static int make(lua_State* L);
};

struct recursive_directory_iterator
{
    template<class... Args>
    recursive_directory_iterator(Args&&... args)
        : iterator{std::forward<Args>(args)...}
    {}

    fs::recursive_directory_iterator iterator;
    bool increment = false;

    static int pop(lua_State* L);
    static int disable_recursion_pending(lua_State* L);
    static int recursion_pending(lua_State* L);
    static int mt_index(lua_State* L);
    static int next(lua_State* L);
    static int make(lua_State* L);
};

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

static int file_clock_time_point_add(lua_State* L)
{
    lua_settop(L, 2);

    auto tp = static_cast<std::chrono::file_clock::time_point*>(
        lua_touserdata(L, 1));
    if (!tp || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_clock_time_point_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    lua_Number secs = luaL_checknumber(L, 2);
    if (std::isnan(secs) || std::isinf(secs)) {
        push(L, std::errc::argument_out_of_domain, "arg", 2);
        return lua_error(L);
    }

    lua_Seconds dur{secs};
    if (
        dur > std::chrono::file_clock::duration::max() ||
        dur < std::chrono::file_clock::duration::min()
    ) {
        push(L, std::errc::value_too_large);
        return lua_error(L);
    }

    try {
        *tp += std::chrono::round<std::chrono::file_clock::duration>(dur);
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }
    return 0;
}

static int file_clock_time_point_sub(lua_State* L)
{
    lua_settop(L, 2);

    auto tp = static_cast<std::chrono::file_clock::time_point*>(
        lua_touserdata(L, 1));
    if (!tp || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_clock_time_point_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    lua_Number secs = luaL_checknumber(L, 2);
    if (std::isnan(secs) || std::isinf(secs)) {
        push(L, std::errc::argument_out_of_domain, "arg", 2);
        return lua_error(L);
    }

    lua_Seconds dur{secs};
    if (
        dur > std::chrono::file_clock::duration::max() ||
        dur < std::chrono::file_clock::duration::min()
    ) {
        push(L, std::errc::value_too_large);
        return lua_error(L);
    }

    try {
        *tp -= std::chrono::round<std::chrono::file_clock::duration>(dur);
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }
    return 0;
}

static int file_clock_time_point_to_system(lua_State* L)
{
    auto tp = static_cast<std::chrono::file_clock::time_point*>(
        lua_touserdata(L, 1));
    if (!tp || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_clock_time_point_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto ret = static_cast<std::chrono::system_clock::time_point*>(
        lua_newuserdata(L, sizeof(std::chrono::system_clock::time_point))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &system_clock_time_point_mt_key);
    setmetatable(L, -2);
    new (ret) std::chrono::system_clock::time_point{};
    // TODO (ifdefs or SFINAE magic): current libstdc++ hasn't got clock_cast
    // yet
    *ret = std::chrono::file_clock::to_sys(*tp);
    return 1;
}

inline int file_clock_time_point_seconds_since_epoch(lua_State* L)
{
    auto tp = static_cast<std::chrono::file_clock::time_point*>(
        lua_touserdata(L, 1));
    lua_pushnumber(L, lua_Seconds{tp->time_since_epoch()}.count());
    return 1;
}

static int file_clock_time_point_mt_index(lua_State* L)
{
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("add"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, file_clock_time_point_add);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("sub"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, file_clock_time_point_sub);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("to_system"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, file_clock_time_point_to_system);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("seconds_since_epoch"),
                file_clock_time_point_seconds_since_epoch
            )
        ),
        [](std::string_view /*key*/, lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        },
        tostringview(L, 2),
        L
    );
}

static int file_clock_time_point_mt_eq(lua_State* L)
{
    auto tp1 = static_cast<std::chrono::file_clock::time_point*>(
        lua_touserdata(L, 1));
    auto tp2 = static_cast<std::chrono::file_clock::time_point*>(
        lua_touserdata(L, 2));
    lua_pushboolean(L, *tp1 == *tp2);
    return 1;
}

static int file_clock_time_point_mt_lt(lua_State* L)
{
    auto tp1 = static_cast<std::chrono::file_clock::time_point*>(
        lua_touserdata(L, 1));
    if (!tp1 || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_clock_time_point_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto tp2 = static_cast<std::chrono::file_clock::time_point*>(
        lua_touserdata(L, 2));
    if (!tp2 || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_clock_time_point_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    lua_pushboolean(L, *tp1 < *tp2);
    return 1;
}

static int file_clock_time_point_mt_le(lua_State* L)
{
    auto tp1 = static_cast<std::chrono::file_clock::time_point*>(
        lua_touserdata(L, 1));
    if (!tp1 || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_clock_time_point_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto tp2 = static_cast<std::chrono::file_clock::time_point*>(
        lua_touserdata(L, 2));
    if (!tp2 || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_clock_time_point_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    lua_pushboolean(L, *tp1 <= *tp2);
    return 1;
}

static int file_clock_time_point_mt_add(lua_State* L)
{
    auto tp = static_cast<std::chrono::file_clock::time_point*>(
        lua_touserdata(L, 1));
    if (!tp || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_clock_time_point_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    lua_Number secs = luaL_checknumber(L, 2);
    if (std::isnan(secs) || std::isinf(secs)) {
        push(L, std::errc::argument_out_of_domain, "arg", 2);
        return lua_error(L);
    }

    lua_Seconds dur{secs};
    if (
        dur > std::chrono::file_clock::duration::max() ||
        dur < std::chrono::file_clock::duration::min()
    ) {
        push(L, std::errc::value_too_large);
        return lua_error(L);
    }

    auto ret = static_cast<std::chrono::file_clock::time_point*>(
        lua_newuserdata(L, sizeof(std::chrono::file_clock::time_point))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &file_clock_time_point_mt_key);
    setmetatable(L, -2);
    new (ret) std::chrono::file_clock::time_point{};

    try {
        *ret = *tp +
            std::chrono::round<std::chrono::file_clock::duration>(dur);
        return 1;
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }
}

static int file_clock_time_point_mt_sub(lua_State* L)
{
    auto tp = static_cast<std::chrono::file_clock::time_point*>(
        lua_touserdata(L, 1));
    if (!tp || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_clock_time_point_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    switch (lua_type(L, 2)) {
    case LUA_TNUMBER: {
        lua_Number secs = lua_tonumber(L, 2);
        if (std::isnan(secs) || std::isinf(secs)) {
            push(L, std::errc::argument_out_of_domain, "arg", 2);
            return lua_error(L);
        }

        lua_Seconds dur{secs};
        if (
            dur > std::chrono::file_clock::duration::max() ||
            dur < std::chrono::file_clock::duration::min()
        ) {
            push(L, std::errc::value_too_large);
            return lua_error(L);
        }

        auto ret = static_cast<std::chrono::file_clock::time_point*>(
            lua_newuserdata(L, sizeof(std::chrono::file_clock::time_point))
        );
        rawgetp(L, LUA_REGISTRYINDEX, &file_clock_time_point_mt_key);
        setmetatable(L, -2);
        new (ret) std::chrono::file_clock::time_point{};

        try {
            *ret = *tp -
                std::chrono::round<std::chrono::file_clock::duration>(dur);
            return 1;
        } catch (const std::system_error& e) {
            push(L, e.code());
            return lua_error(L);
        } catch (const std::exception& e) {
            lua_pushstring(L, e.what());
            return lua_error(L);
        }
    }
    case LUA_TUSERDATA: {
        auto tp2 = static_cast<std::chrono::file_clock::time_point*>(
            lua_touserdata(L, 2));
        if (!tp2 || !lua_getmetatable(L, 2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &file_clock_time_point_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        try {
            lua_pushnumber(L, lua_Seconds{*tp - *tp2}.count());
            return 1;
        } catch (const std::system_error& e) {
            push(L, e.code());
            return lua_error(L);
        } catch (const std::exception& e) {
            lua_pushstring(L, e.what());
            return lua_error(L);
        }
    }
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
}

inline int space_info_capacity(lua_State* L)
{
    auto space = static_cast<fs::space_info*>(lua_touserdata(L, 1));
    lua_pushinteger(L, space->capacity);
    return 1;
}

inline int space_info_free(lua_State* L)
{
    auto space = static_cast<fs::space_info*>(lua_touserdata(L, 1));
    lua_pushinteger(L, space->free);
    return 1;
}

inline int space_info_available(lua_State* L)
{
    auto space = static_cast<fs::space_info*>(lua_touserdata(L, 1));
    lua_pushinteger(L, space->available);
    return 1;
}

static int space_info_mt_index(lua_State* L)
{
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(BOOST_HANA_STRING("capacity"), space_info_capacity),
            hana::make_pair(BOOST_HANA_STRING("free"), space_info_free),
            hana::make_pair(
                BOOST_HANA_STRING("available"), space_info_available)
        ),
        [](std::string_view /*key*/, lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        },
        tostringview(L, 2),
        L
    );
}

static int space_info_mt_eq(lua_State* L)
{
    auto sp1 = static_cast<fs::space_info*>(lua_touserdata(L, 1));
    auto sp2 = static_cast<fs::space_info*>(lua_touserdata(L, 2));
    lua_pushboolean(L, *sp1 == *sp2);
    return 1;
}

inline int file_status_type(lua_State* L)
{
    auto st = static_cast<fs::file_status*>(lua_touserdata(L, 1));
    switch (st->type()) {
    case fs::file_type::none:
        lua_pushnil(L);
        return 1;
    case fs::file_type::not_found:
        lua_pushliteral(L, "not_found");
        return 1;
    case fs::file_type::regular:
        lua_pushliteral(L, "regular");
        return 1;
    case fs::file_type::directory:
        lua_pushliteral(L, "directory");
        return 1;
    case fs::file_type::symlink:
        lua_pushliteral(L, "symlink");
        return 1;
    case fs::file_type::block:
        lua_pushliteral(L, "block");
        return 1;
    case fs::file_type::character:
        lua_pushliteral(L, "character");
        return 1;
    case fs::file_type::fifo:
        lua_pushliteral(L, "fifo");
        return 1;
    case fs::file_type::socket:
        lua_pushliteral(L, "socket");
        return 1;
#if BOOST_OS_WINDOWS
    case fs::file_type::junction:
        lua_pushliteral(L, "junction");
        return 1;
#endif // BOOST_OS_WINDOWS
    case fs::file_type::unknown:
        // by avoiding an explicit `default` case we get compiler warnings in
        // case we miss any
        break;
    }

    lua_pushliteral(L, "unknown");
    return 1;
}

inline int file_status_mode(lua_State* L)
{
    auto st = static_cast<fs::file_status*>(lua_touserdata(L, 1));
    if (st->permissions() == fs::perms::unknown) {
        lua_pushliteral(L, "unknown");
        return 1;
    }
    auto p = static_cast<std::underlying_type_t<fs::perms>>(st->permissions());
    lua_pushinteger(L, p);
    return 1;
}

static int file_status_mt_index(lua_State* L)
{
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(BOOST_HANA_STRING("type"), file_status_type),
            hana::make_pair(BOOST_HANA_STRING("mode"), file_status_mode)
        ),
        [](std::string_view /*key*/, lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        },
        tostringview(L, 2),
        L
    );
}

static int file_status_mt_eq(lua_State* L)
{
    auto st1 = static_cast<fs::file_status*>(lua_touserdata(L, 1));
    auto st2 = static_cast<fs::file_status*>(lua_touserdata(L, 2));
    lua_pushboolean(L, *st1 == *st2);
    return 1;
}

static int directory_entry_refresh(lua_State* L)
{
    auto entry = static_cast<fs::directory_entry*>(lua_touserdata(L, 1));
    if (!entry || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &directory_entry_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    std::error_code ec;
    entry->refresh(ec);
    if (ec) {
        push(L, ec);

        lua_pushliteral(L, "path1");
        {
            auto path = static_cast<fs::path*>(
                lua_newuserdata(L, sizeof(fs::path)));
            rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
            setmetatable(L, -2);
            new (path) fs::path{};
            *path = entry->path();
        }
        lua_rawset(L, -3);

        return lua_error(L);
    }
    return 0;
}

inline int directory_entry_path(lua_State* L)
{
    auto entry = static_cast<fs::directory_entry*>(lua_touserdata(L, 1));
    auto path = static_cast<fs::path*>(lua_newuserdata(L, sizeof(fs::path)));
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    setmetatable(L, -2);
    new (path) fs::path{};
    *path = entry->path();
    return 1;
}

inline int directory_entry_file_size(lua_State* L)
{
    auto entry = static_cast<fs::directory_entry*>(lua_touserdata(L, 1));

    std::error_code ec;
    auto ret = entry->file_size(ec);
    if (ec) {
        push(L, ec);

        lua_pushliteral(L, "path1");
        {
            auto path = static_cast<fs::path*>(
                lua_newuserdata(L, sizeof(fs::path)));
            rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
            setmetatable(L, -2);
            new (path) fs::path{};
            *path = entry->path();
        }
        lua_rawset(L, -3);

        return lua_error(L);
    }
    lua_pushinteger(L, ret);
    return 1;
}

inline int directory_entry_hard_link_count(lua_State* L)
{
    auto entry = static_cast<fs::directory_entry*>(lua_touserdata(L, 1));

    std::error_code ec;
    auto ret = entry->hard_link_count(ec);
    if (ec) {
        push(L, ec);

        lua_pushliteral(L, "path1");
        {
            auto path = static_cast<fs::path*>(
                lua_newuserdata(L, sizeof(fs::path)));
            rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
            setmetatable(L, -2);
            new (path) fs::path{};
            *path = entry->path();
        }
        lua_rawset(L, -3);

        return lua_error(L);
    }
    lua_pushinteger(L, ret);
    return 1;
}

inline int directory_entry_last_write_time(lua_State* L)
{
    auto entry = static_cast<fs::directory_entry*>(lua_touserdata(L, 1));

    std::error_code ec;
    auto ret = entry->last_write_time(ec);
    if (ec) {
        push(L, ec);

        lua_pushliteral(L, "path1");
        {
            auto path = static_cast<fs::path*>(
                lua_newuserdata(L, sizeof(fs::path)));
            rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
            setmetatable(L, -2);
            new (path) fs::path{};
            *path = entry->path();
        }
        lua_rawset(L, -3);

        return lua_error(L);
    }

    auto tp = static_cast<std::chrono::file_clock::time_point*>(
        lua_newuserdata(L, sizeof(std::chrono::file_clock::time_point))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &file_clock_time_point_mt_key);
    setmetatable(L, -2);
    new (tp) std::chrono::file_clock::time_point{ret};
    return 1;
}

inline int directory_entry_status(lua_State* L)
{
    auto entry = static_cast<fs::directory_entry*>(lua_touserdata(L, 1));

    std::error_code ec;
    auto ret = entry->status(ec);
    if (ec) {
        push(L, ec);

        lua_pushliteral(L, "path1");
        {
            auto path = static_cast<fs::path*>(
                lua_newuserdata(L, sizeof(fs::path)));
            rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
            setmetatable(L, -2);
            new (path) fs::path{};
            *path = entry->path();
        }
        lua_rawset(L, -3);

        return lua_error(L);
    }

    auto st = static_cast<fs::file_status*>(
        lua_newuserdata(L, sizeof(fs::file_status))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &file_status_mt_key);
    setmetatable(L, -2);
    new (st) fs::file_status{ret};
    return 1;
}

inline int directory_entry_symlink_status(lua_State* L)
{
    auto entry = static_cast<fs::directory_entry*>(lua_touserdata(L, 1));

    std::error_code ec;
    auto ret = entry->symlink_status(ec);
    if (ec) {
        push(L, ec);

        lua_pushliteral(L, "path1");
        {
            auto path = static_cast<fs::path*>(
                lua_newuserdata(L, sizeof(fs::path)));
            rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
            setmetatable(L, -2);
            new (path) fs::path{};
            *path = entry->path();
        }
        lua_rawset(L, -3);

        return lua_error(L);
    }

    auto st = static_cast<fs::file_status*>(
        lua_newuserdata(L, sizeof(fs::file_status))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &file_status_mt_key);
    setmetatable(L, -2);
    new (st) fs::file_status{ret};
    return 1;
}

static int directory_entry_mt_index(lua_State* L)
{
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("refresh"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, directory_entry_refresh);
                    return 1;
                }
            ),
            hana::make_pair(BOOST_HANA_STRING("path"), directory_entry_path),
            hana::make_pair(
                BOOST_HANA_STRING("file_size"), directory_entry_file_size),
            hana::make_pair(
                BOOST_HANA_STRING("hard_link_count"),
                directory_entry_hard_link_count),
            hana::make_pair(
                BOOST_HANA_STRING("last_write_time"),
                directory_entry_last_write_time),
            hana::make_pair(
                BOOST_HANA_STRING("status"), directory_entry_status),
            hana::make_pair(
                BOOST_HANA_STRING("symlink_status"),
                directory_entry_symlink_status)
        ),
        [](std::string_view /*key*/, lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        },
        tostringview(L, 2),
        L
    );
}

int directory_iterator::next(lua_State* L)
{
    auto self = static_cast<directory_iterator*>(
        lua_touserdata(L, lua_upvalueindex(1)));

    if (self->iterator == fs::directory_iterator{})
        return 0;

    if (self->increment) {
        std::error_code ec;
        self->iterator.increment(ec);
        if (ec) {
            push(L, ec);
            return lua_error(L);
        }

        if (self->iterator == fs::directory_iterator{})
            return 0;
    } else {
        self->increment = true;
    }

    auto ret = static_cast<fs::directory_entry*>(
        lua_newuserdata(L, sizeof(fs::directory_entry)));
    rawgetp(L, LUA_REGISTRYINDEX, &directory_entry_mt_key);
    setmetatable(L, -2);
    new (ret) fs::directory_entry{};
    *ret = *(self->iterator);
    return 1;
}

int directory_iterator::make(lua_State* L)
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

    fs::directory_options options = fs::directory_options::none;

    switch (lua_type(L, 2)) {
    case LUA_TNIL:
        break;
    case LUA_TTABLE:
        lua_getfield(L, 2, "skip_permission_denied");
        switch (lua_type(L, -1)) {
        case LUA_TNIL:
            break;
        case LUA_TBOOLEAN:
            if (lua_toboolean(L, -1))
                options |= fs::directory_options::skip_permission_denied;
            break;
        default:
            push(L, std::errc::invalid_argument,
                 "arg", "skip_permission_denied");
            return lua_error(L);
        }
        break;
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    {
        std::error_code ec;
        auto iter = static_cast<directory_iterator*>(
            lua_newuserdata(L, sizeof(directory_iterator)));
        rawgetp(L, LUA_REGISTRYINDEX, &directory_iterator_mt_key);
        setmetatable(L, -2);
        new (iter) directory_iterator{*path, options, ec};

        if (ec) {
            push(L, ec);
            lua_pushliteral(L, "path1");
            lua_pushvalue(L, 1);
            lua_rawset(L, -3);
            return lua_error(L);
        }
    }
    lua_pushcclosure(L, next, 1);
    return 1;
}

int recursive_directory_iterator::pop(lua_State* L)
{
    auto self = static_cast<recursive_directory_iterator*>(
        lua_touserdata(L, 1));
    if (!self || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &recursive_directory_iterator_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    std::error_code ec;
    self->iterator.pop(ec);
    if (ec) {
        push(L, ec);
        return lua_error(L);
    }
    return 0;
}

int recursive_directory_iterator::disable_recursion_pending(lua_State* L)
{
    auto self = static_cast<recursive_directory_iterator*>(
        lua_touserdata(L, 1));
    if (!self || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &recursive_directory_iterator_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    if (self->iterator == fs::recursive_directory_iterator{}) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    try {
        self->iterator.disable_recursion_pending();
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }
    return 0;
}

int recursive_directory_iterator::recursion_pending(lua_State* L)
{
    auto self = static_cast<recursive_directory_iterator*>(
        lua_touserdata(L, 1));
    if (self->iterator == fs::recursive_directory_iterator{}) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    lua_pushboolean(L, self->iterator.recursion_pending());
    return 1;
}

int recursive_directory_iterator::mt_index(lua_State* L)
{
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("pop"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, pop);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("disable_recursion_pending"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, disable_recursion_pending);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("recursion_pending"), recursion_pending
            )
        ),
        [](std::string_view /*key*/, lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        },
        tostringview(L, 2),
        L
    );
}

int recursive_directory_iterator::next(lua_State* L)
{
    auto self = static_cast<recursive_directory_iterator*>(
        lua_touserdata(L, lua_upvalueindex(1)));

    if (self->iterator == fs::recursive_directory_iterator{})
        return 0;

    if (self->increment) {
        std::error_code ec;
        self->iterator.increment(ec);
        if (ec) {
            push(L, ec);
            return lua_error(L);
        }

        if (self->iterator == fs::recursive_directory_iterator{})
            return 0;
    } else {
        self->increment = true;
    }

    auto ret = static_cast<fs::directory_entry*>(
        lua_newuserdata(L, sizeof(fs::directory_entry)));
    rawgetp(L, LUA_REGISTRYINDEX, &directory_entry_mt_key);
    setmetatable(L, -2);
    new (ret) fs::directory_entry{};
    *ret = *(self->iterator);

    lua_pushinteger(L, self->iterator.depth());

    return 2;
}

int recursive_directory_iterator::make(lua_State* L)
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

    fs::directory_options options = fs::directory_options::none;

    switch (lua_type(L, 2)) {
    case LUA_TNIL:
        break;
    case LUA_TTABLE:
        lua_getfield(L, 2, "skip_permission_denied");
        switch (lua_type(L, -1)) {
        case LUA_TNIL:
            break;
        case LUA_TBOOLEAN:
            if (lua_toboolean(L, -1))
                options |= fs::directory_options::skip_permission_denied;
            break;
        default:
            push(L, std::errc::invalid_argument,
                 "arg", "skip_permission_denied");
            return lua_error(L);
        }

        lua_getfield(L, 2, "follow_directory_symlink");
        switch (lua_type(L, -1)) {
        case LUA_TNIL:
            break;
        case LUA_TBOOLEAN:
            if (lua_toboolean(L, -1))
                options |= fs::directory_options::follow_directory_symlink;
            break;
        default:
            push(L, std::errc::invalid_argument,
                 "arg", "follow_directory_symlink");
            return lua_error(L);
        }
        break;
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    {
        std::error_code ec;
        auto iter = static_cast<recursive_directory_iterator*>(
            lua_newuserdata(L, sizeof(recursive_directory_iterator)));
        rawgetp(L, LUA_REGISTRYINDEX, &recursive_directory_iterator_mt_key);
        setmetatable(L, -2);
        new (iter) recursive_directory_iterator{*path, options, ec};

        if (ec) {
            push(L, ec);
            lua_pushliteral(L, "path1");
            lua_pushvalue(L, 1);
            lua_rawset(L, -3);
            return lua_error(L);
        }
    }
    lua_pushvalue(L, -1);
    lua_pushcclosure(L, next, 1);
    lua_insert(L, -2);
    return 2;
}

static int file_clock_from_system(lua_State* L)
{
    auto tp = static_cast<std::chrono::system_clock::time_point*>(
        lua_touserdata(L, 1));
    if (!tp || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &system_clock_time_point_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto ret = static_cast<std::chrono::file_clock::time_point*>(
        lua_newuserdata(L, sizeof(std::chrono::file_clock::time_point))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &file_clock_time_point_mt_key);
    setmetatable(L, -2);
    new (ret) std::chrono::file_clock::time_point{};
    // TODO (ifdefs or SFINAE magic): current libstdc++ hasn't got clock_cast
    // yet
    *ret = std::chrono::file_clock::from_sys(*tp);
    return 1;
}

static int absolute(lua_State* L)
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

    std::error_code ec;
    *ret = fs::absolute(*path, ec);
    if (ec) {
        push(L, ec);
        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }
    return 1;
}

static int canonical(lua_State* L)
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

    std::error_code ec;
    *ret = fs::canonical(*path, ec);
    if (ec) {
        push(L, ec);
        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }
    return 1;
}

static int weakly_canonical(lua_State* L)
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

    std::error_code ec;
    *ret = fs::weakly_canonical(*path, ec);
    if (ec) {
        push(L, ec);
        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }
    return 1;
}

static int relative(lua_State* L)
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
    case LUA_TNIL: {
        std::error_code ec;
        path2 = fs::current_path(ec);
        if (ec) {
            push(L, ec);
            lua_pushliteral(L, "path1");
            lua_pushvalue(L, 1);
            lua_rawset(L, -3);
            return lua_error(L);
        }
        break;
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

    std::error_code ec;
    *ret = fs::relative(*path, path2, ec);
    if (ec) {
        push(L, ec);

        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);

        lua_pushliteral(L, "path2");
        *ret = path2;
        lua_pushvalue(L, -3);
        lua_rawset(L, -3);

        return lua_error(L);
    }
    return 1;
}

static int proximate(lua_State* L)
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
    case LUA_TNIL: {
        std::error_code ec;
        path2 = fs::current_path(ec);
        if (ec) {
            push(L, ec);
            lua_pushliteral(L, "path1");
            lua_pushvalue(L, 1);
            lua_rawset(L, -3);
            return lua_error(L);
        }
        break;
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

    std::error_code ec;
    *ret = fs::proximate(*path, path2, ec);
    if (ec) {
        push(L, ec);

        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);

        lua_pushliteral(L, "path2");
        *ret = path2;
        lua_pushvalue(L, -3);
        lua_rawset(L, -3);

        return lua_error(L);
    }
    return 1;
}

static int last_write_time(lua_State* L)
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

    switch (lua_type(L, 2)) {
    case LUA_TNIL: {
        auto tp = static_cast<std::chrono::file_clock::time_point*>(
            lua_newuserdata(L, sizeof(std::chrono::file_clock::time_point))
        );
        rawgetp(L, LUA_REGISTRYINDEX, &file_clock_time_point_mt_key);
        setmetatable(L, -2);
        new (tp) std::chrono::file_clock::time_point{};
        std::error_code ec;
        *tp = fs::last_write_time(*path, ec);
        if (ec) {
            push(L, ec);
            lua_pushliteral(L, "path1");
            lua_pushvalue(L, 1);
            lua_rawset(L, -3);
            return lua_error(L);
        }
        return 1;
    }
    case LUA_TUSERDATA: {
        auto tp = static_cast<std::chrono::file_clock::time_point*>(
            lua_touserdata(L, 2));
        if (!tp || !lua_getmetatable(L, 2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &file_clock_time_point_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        std::error_code ec;
        fs::last_write_time(*path, *tp, ec);
        if (ec) {
            push(L, ec);
            lua_pushliteral(L, "path1");
            lua_pushvalue(L, 1);
            lua_rawset(L, -3);
            return lua_error(L);
        }
        return 0;
    }
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
}

static int create_directory(lua_State* L)
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

    switch (lua_type(L, 2)) {
    case LUA_TNIL: {
        std::error_code ec;
        bool ret = fs::create_directory(*path, ec);
        if (ec) {
            push(L, ec);
            lua_pushliteral(L, "path1");
            lua_pushvalue(L, 1);
            lua_rawset(L, -3);
            return lua_error(L);
        }
        lua_pushboolean(L, ret);
        return 1;
    }
    case LUA_TUSERDATA: {
        auto path2 = static_cast<fs::path*>(lua_touserdata(L, 2));
        if (!path2 || !lua_getmetatable(L, 2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        std::error_code ec;
        bool ret = fs::create_directory(*path, *path2, ec);
        if (ec) {
            push(L, ec);

            lua_pushliteral(L, "path1");
            lua_pushvalue(L, 1);
            lua_rawset(L, -3);

            lua_pushliteral(L, "path2");
            lua_pushvalue(L, 2);
            lua_rawset(L, -3);

            return lua_error(L);
        }
        lua_pushboolean(L, ret);
        return 1;
    }
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
}

static int create_directories(lua_State* L)
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

    std::error_code ec;
    bool ret = fs::create_directories(*path, ec);
    if (ec) {
        push(L, ec);
        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }
    lua_pushboolean(L, ret);
    return 1;
}

static int file_size(lua_State* L)
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

    std::error_code ec;
    auto ret = fs::file_size(*path, ec);
    if (ec) {
        push(L, ec);
        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }
    lua_pushinteger(L, ret);
    return 1;
}

static int hard_link_count(lua_State* L)
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

    std::error_code ec;
    auto ret = fs::hard_link_count(*path, ec);
    if (ec) {
        push(L, ec);
        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }
    lua_pushinteger(L, ret);
    return 1;
}

static int chmod(lua_State* L)
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

    std::error_code ec;
    fs::permissions(*path, static_cast<fs::perms>(luaL_checkinteger(L, 2)), ec);
    if (ec) {
        push(L, ec);
        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }
    return 0;
}

static int lchmod(lua_State* L)
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

    std::error_code ec;
    fs::permissions(
        *path, static_cast<fs::perms>(luaL_checkinteger(L, 2)),
        fs::perm_options::replace | fs::perm_options::nofollow, ec);
    if (ec) {
        push(L, ec);
        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }
    return 0;
}

static int read_symlink(lua_State* L)
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

    std::error_code ec;
    *ret = fs::read_symlink(*path, ec);
    if (ec) {
        push(L, ec);
        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }
    return 1;
}

static int remove(lua_State* L)
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

    std::error_code ec;
    bool ret = fs::remove(*path, ec);
    if (ec) {
        push(L, ec);
        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }
    lua_pushboolean(L, ret);
    return 1;
}

static int remove_all(lua_State* L)
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

    std::error_code ec;
    auto ret = fs::remove_all(*path, ec);
    if (ec) {
        push(L, ec);
        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }
    lua_pushinteger(L, ret);
    return 1;
}

static int is_empty(lua_State* L)
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

    std::error_code ec;
    bool ret = fs::is_empty(*path, ec);
    if (ec) {
        push(L, ec);
        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }
    lua_pushboolean(L, ret);
    return 1;
}

static int current_working_directory(lua_State* L)
{
    lua_settop(L, 1);

    if (lua_isnil(L, 1)) {
        auto path = static_cast<fs::path*>(
            lua_newuserdata(L, sizeof(fs::path)));
        rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
        setmetatable(L, -2);
        new (path) fs::path{};

        std::error_code ec;
        *path = fs::current_path(ec);
        if (ec) {
            push(L, ec);
            return lua_error(L);
        }
        return 1;
    }

    auto& vm_ctx = get_vm_context(L);
    if (!vm_ctx.is_master()) {
        // we intentionally leave "path1" out as the path has nothing to do with
        // the failure here
        push(L, std::errc::operation_not_permitted);
        return lua_error(L);
    }

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

    std::error_code ec;
    fs::current_path(*path, ec);
    if (ec) {
        push(L, ec);
        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }
    return 0;
}

static int space(lua_State* L)
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

    std::error_code ec;
    auto ret = fs::space(*path, ec);
    if (ec) {
        push(L, ec);

        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);

        return lua_error(L);
    }

    auto space = static_cast<fs::space_info*>(
        lua_newuserdata(L, sizeof(fs::space_info))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &space_info_mt_key);
    setmetatable(L, -2);
    new (space) fs::space_info{ret};
    return 1;
}

static int status(lua_State* L)
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

    auto ret = static_cast<fs::file_status*>(
        lua_newuserdata(L, sizeof(fs::file_status))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &file_status_mt_key);
    setmetatable(L, -2);
    new (ret) fs::file_status{};

    std::error_code ec;
    *ret = fs::status(*path, ec);
    if (ec) {
        push(L, ec);
        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }
    return 1;
}

static int symlink_status(lua_State* L)
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

    auto ret = static_cast<fs::file_status*>(
        lua_newuserdata(L, sizeof(fs::file_status))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &file_status_mt_key);
    setmetatable(L, -2);
    new (ret) fs::file_status{};

    std::error_code ec;
    *ret = fs::symlink_status(*path, ec);
    if (ec) {
        push(L, ec);
        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }
    return 1;
}

static int temp_directory_path(lua_State* L)
{
    lua_settop(L, 0);

    auto path = static_cast<fs::path*>(lua_newuserdata(L, sizeof(fs::path)));
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    setmetatable(L, -2);
    new (path) fs::path{};

    try {
        *path = fs::temp_directory_path();
        return 1;
    } catch (const fs::filesystem_error& e) {
        push(L, e.code());

        *path = e.path1();
        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);

        return lua_error(L);
    }
}

static int filesystem_umask(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);
    if (!vm_ctx.is_master()) {
        push(L, std::errc::operation_not_permitted);
        return lua_error(L);
    }

    mode_t res = umask(luaL_checkinteger(L, 1));
    lua_pushinteger(L, res);
    return 1;
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

    lua_pushlightuserdata(L, &file_clock_time_point_mt_key);
    {
        static_assert(std::is_trivially_destructible_v<
            std::chrono::file_clock::time_point>);

        lua_createtable(L, /*narr=*/0, /*nrec=*/7);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "filesystem.clock.time_point");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, file_clock_time_point_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__eq");
        lua_pushcfunction(L, file_clock_time_point_mt_eq);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__lt");
        lua_pushcfunction(L, file_clock_time_point_mt_lt);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__le");
        lua_pushcfunction(L, file_clock_time_point_mt_le);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__add");
        lua_pushcfunction(L, file_clock_time_point_mt_add);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__sub");
        lua_pushcfunction(L, file_clock_time_point_mt_sub);
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

    lua_pushlightuserdata(L, &directory_iterator_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/1);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<directory_iterator>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &recursive_directory_iterator_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "filesystem.recursive_directory_iterator");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, recursive_directory_iterator::mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<recursive_directory_iterator>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &space_info_mt_key);
    {
        static_assert(std::is_trivially_destructible_v<fs::space_info>);

        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "filesystem.space_info");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, space_info_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__eq");
        lua_pushcfunction(L, space_info_mt_eq);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &file_status_mt_key);
    {
        static_assert(std::is_trivially_destructible_v<fs::file_status>);

        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "filesystem.file_status");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, file_status_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__eq");
        lua_pushcfunction(L, file_status_mt_eq);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &directory_entry_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "filesystem.directory_entry");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, directory_entry_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<fs::directory_entry>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &filesystem_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/26);

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

        lua_pushliteral(L, "clock");
        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/1);

            lua_pushliteral(L, "from_system");
            lua_pushcfunction(L, file_clock_from_system);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);

        lua_pushliteral(L, "directory_iterator");
        lua_pushcfunction(L, directory_iterator::make);
        lua_rawset(L, -3);

        lua_pushliteral(L, "recursive_directory_iterator");
        lua_pushcfunction(L, recursive_directory_iterator::make);
        lua_rawset(L, -3);

        lua_pushliteral(L, "absolute");
        lua_pushcfunction(L, absolute);
        lua_rawset(L, -3);

        lua_pushliteral(L, "canonical");
        lua_pushcfunction(L, canonical);
        lua_rawset(L, -3);

        lua_pushliteral(L, "weakly_canonical");
        lua_pushcfunction(L, weakly_canonical);
        lua_rawset(L, -3);

        lua_pushliteral(L, "relative");
        lua_pushcfunction(L, relative);
        lua_rawset(L, -3);

        lua_pushliteral(L, "proximate");
        lua_pushcfunction(L, proximate);
        lua_rawset(L, -3);

        lua_pushliteral(L, "last_write_time");
        lua_pushcfunction(L, last_write_time);
        lua_rawset(L, -3);

        lua_pushliteral(L, "create_directory");
        lua_pushcfunction(L, create_directory);
        lua_rawset(L, -3);

        lua_pushliteral(L, "create_directories");
        lua_pushcfunction(L, create_directories);
        lua_rawset(L, -3);

        lua_pushliteral(L, "file_size");
        lua_pushcfunction(L, file_size);
        lua_rawset(L, -3);

        lua_pushliteral(L, "hard_link_count");
        lua_pushcfunction(L, hard_link_count);
        lua_rawset(L, -3);

        lua_pushliteral(L, "chmod");
        lua_pushcfunction(L, chmod);
        lua_rawset(L, -3);

        lua_pushliteral(L, "lchmod");
        lua_pushcfunction(L, lchmod);
        lua_rawset(L, -3);

        lua_pushliteral(L, "read_symlink");
        lua_pushcfunction(L, read_symlink);
        lua_rawset(L, -3);

        lua_pushliteral(L, "remove");
        lua_pushcfunction(L, remove);
        lua_rawset(L, -3);

        lua_pushliteral(L, "remove_all");
        lua_pushcfunction(L, remove_all);
        lua_rawset(L, -3);

        lua_pushliteral(L, "is_empty");
        lua_pushcfunction(L, is_empty);
        lua_rawset(L, -3);

        lua_pushliteral(L, "current_working_directory");
        lua_pushcfunction(L, current_working_directory);
        lua_rawset(L, -3);

        lua_pushliteral(L, "space");
        lua_pushcfunction(L, space);
        lua_rawset(L, -3);

        lua_pushliteral(L, "status");
        lua_pushcfunction(L, status);
        lua_rawset(L, -3);

        lua_pushliteral(L, "symlink_status");
        lua_pushcfunction(L, symlink_status);
        lua_rawset(L, -3);

        lua_pushliteral(L, "temp_directory_path");
        lua_pushcfunction(L, temp_directory_path);
        lua_rawset(L, -3);

        lua_pushliteral(L, "umask");
        lua_pushcfunction(L, filesystem_umask);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);
}

} // namespace emilua
