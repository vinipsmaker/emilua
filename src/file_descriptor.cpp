/* Copyright (c) 2022 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#include <emilua/file_descriptor.hpp>

#include <charconv>

#include <boost/hana/integral_constant.hpp>
#include <boost/hana/greater.hpp>
#include <boost/hana/string.hpp>
#include <boost/hana/plus.hpp>
#include <boost/hana/div.hpp>

#include <boost/scope_exit.hpp>
#include <boost/config.hpp>

#if BOOST_OS_LINUX
#include <sys/capability.h>
#include <emilua/system.hpp>
#endif // BOOST_OS_LINUX

namespace emilua {

char file_descriptor_mt_key;

static int file_descriptor_close(lua_State* L)
{
    auto handle = static_cast<file_descriptor_handle*>(lua_touserdata(L, 1));
    if (!handle || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    if (*handle == INVALID_FILE_DESCRIPTOR) {
        push(L, std::errc::device_or_resource_busy);
        return lua_error(L);
    }

    lua_pushnil(L);
    setmetatable(L, 1);

    int res = close(*handle);
    boost::ignore_unused(res);

    return 0;
}

static int file_descriptor_dup(lua_State* L)
{
    auto oldhandle = static_cast<file_descriptor_handle*>(lua_touserdata(L, 1));
    if (!oldhandle || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    if (*oldhandle == INVALID_FILE_DESCRIPTOR) {
        push(L, std::errc::device_or_resource_busy);
        return lua_error(L);
    }

    int newfd = dup(*oldhandle);
    BOOST_SCOPE_EXIT_ALL(&) {
        if (newfd != -1) {
            int res = close(newfd);
            boost::ignore_unused(res);
        }
    };
    if (newfd == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }

    auto newhandle = static_cast<file_descriptor_handle*>(
        lua_newuserdata(L, sizeof(file_descriptor_handle))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
    setmetatable(L, -2);

    *newhandle = newfd;
    newfd = -1;
    return 1;
}

#if BOOST_OS_LINUX
static int file_descriptor_cap_get(lua_State* L)
{
    auto handle = static_cast<file_descriptor_handle*>(lua_touserdata(L, 1));
    if (!handle || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    if (*handle == INVALID_FILE_DESCRIPTOR) {
        push(L, std::errc::device_or_resource_busy);
        return lua_error(L);
    }

    cap_t caps = cap_get_fd(*handle);
    if (caps == NULL) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }
    BOOST_SCOPE_EXIT_ALL(&) {
        if (caps != NULL)
            cap_free(caps);
    };

    auto& caps2 = *static_cast<cap_t*>(lua_newuserdata(L, sizeof(cap_t)));
    rawgetp(L, LUA_REGISTRYINDEX, &linux_capabilities_mt_key);
    setmetatable(L, -2);
    caps2 = caps;
    caps = NULL;

    return 1;
}

static int file_descriptor_cap_set(lua_State* L)
{
    auto handle = static_cast<file_descriptor_handle*>(lua_touserdata(L, 1));
    if (!handle || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    if (*handle == INVALID_FILE_DESCRIPTOR) {
        push(L, std::errc::device_or_resource_busy);
        return lua_error(L);
    }

    auto caps = static_cast<cap_t*>(lua_touserdata(L, 2));
    if (!caps || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &linux_capabilities_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    if (cap_set_fd(*handle, *caps) == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }
    return 0;
}
#endif // BOOST_OS_LINUX

static int file_descriptor_mt_index(lua_State* L)
{
    auto key = tostringview(L, 2);
    if (key == "close") {
        lua_pushcfunction(L, file_descriptor_close);
        return 1;
    } else if (key == "dup") {
        lua_pushcfunction(L, file_descriptor_dup);
        return 1;
#if BOOST_OS_LINUX
    } else if (key == "cap_get") {
        lua_pushcfunction(L, file_descriptor_cap_get);
        return 1;
    } else if (key == "cap_set") {
        lua_pushcfunction(L, file_descriptor_cap_set);
        return 1;
#endif // BOOST_OS_LINUX
    } else {
        push(L, errc::bad_index, "index", 2);
        return lua_error(L);
    }
}

static int file_descriptor_mt_tostring(lua_State* L)
{
    auto& handle = *static_cast<file_descriptor_handle*>(lua_touserdata(L, 1));

    if (handle == INVALID_FILE_DESCRIPTOR) {
        push(L, std::errc::device_or_resource_busy);
        return lua_error(L);
    }

    // Paranoia. Apparently the kernel side disagrees about what should be the
    // file descriptor's underlying type (cf. close_range(2)) so negative values
    // could be possible (very unlikely as EMFILE should still hinder them
    // anyway).
    if (BOOST_UNLIKELY(handle < 0)) {
        lua_pushfstring(L, "/dev/fd/%i", handle);
        return 1;
    }

    auto prefix = BOOST_HANA_STRING("/dev/fd/");
    constexpr auto max_digits = hana::second(hana::while_(
        [](auto s) { return hana::first(s) > hana::int_c<0>; },
        hana::make_pair(
            /*i=*/hana::int_c<std::numeric_limits<int>::max()>,
            /*max_digits=*/hana::int_c<0>),
        [](auto s) {
            return hana::make_pair(
                hana::first(s) / hana::int_c<10>,
                hana::second(s) + hana::int_c<1>);
        }
    ));

    std::array<char, hana::length(prefix).value + max_digits.value> buf;
    std::memcpy(buf.data(), prefix.c_str(), hana::length(prefix));
    auto s_size = std::to_chars(
        buf.data() + hana::length(prefix).value,
        buf.data() + buf.size(),
        handle).ptr - buf.data();

    lua_pushlstring(L, buf.data(), s_size);
    return 1;
}

static int file_descriptor_mt_gc(lua_State* L)
{
    auto& handle = *static_cast<file_descriptor_handle*>(lua_touserdata(L, 1));
    if (handle == INVALID_FILE_DESCRIPTOR)
        return 0;

    int res = close(handle);
    boost::ignore_unused(res);
    return 0;
}

void init_file_descriptor(lua_State* L)
{
    lua_pushlightuserdata(L, &file_descriptor_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/4);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "file_descriptor");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, file_descriptor_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__tostring");
        lua_pushcfunction(L, file_descriptor_mt_tostring);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, file_descriptor_mt_gc);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);
}

} // namespace emilua
