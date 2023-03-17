/* Copyright (c) 2022 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#include <boost/asio/random_access_file.hpp>
#include <boost/asio/stream_file.hpp>
#include <boost/scope_exit.hpp>

#include <emilua/file_descriptor.hpp>
#include <emilua/dispatch_table.hpp>
#include <emilua/async_base.hpp>
#include <emilua/filesystem.hpp>
#include <emilua/byte_span.hpp>
#include <emilua/unix.hpp>

namespace emilua {

extern unsigned char write_all_at_bytecode[];
extern std::size_t write_all_at_bytecode_size;
extern unsigned char write_at_least_at_bytecode[];
extern std::size_t write_at_least_at_bytecode_size;
extern unsigned char read_all_at_bytecode[];
extern std::size_t read_all_at_bytecode_size;
extern unsigned char read_at_least_at_bytecode[];
extern std::size_t read_at_least_at_bytecode_size;

char file_key;
char file_stream_mt_key;
char file_random_access_mt_key;

static char stream_read_some_key;
static char stream_write_some_key;
static char random_access_read_some_at_key;
static char random_access_write_some_at_key;

int byte_span_non_member_append(lua_State* L);

static int stream_open(lua_State* L)
{
    luaL_checktype(L, 3, LUA_TNUMBER);

    auto file = static_cast<asio::stream_file*>(lua_touserdata(L, 1));
    if (!file || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_stream_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    std::string path;
    try {
        auto p = static_cast<std::filesystem::path*>(lua_touserdata(L, 2));
        if (!p || !lua_getmetatable(L, 2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        path = p->string();
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }

    auto flags = static_cast<asio::file_base::flags>(lua_tointeger(L, 3));

    boost::system::error_code ec;
    file->open(path, flags, ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int stream_close(lua_State* L)
{
    auto file = static_cast<asio::stream_file*>(lua_touserdata(L, 1));
    if (!file || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_stream_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    file->close(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int stream_cancel(lua_State* L)
{
    auto file = static_cast<asio::stream_file*>(lua_touserdata(L, 1));
    if (!file || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_stream_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    file->cancel(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

#if BOOST_OS_UNIX
static int stream_assign(lua_State* L)
{
    auto file = static_cast<asio::stream_file*>(lua_touserdata(L, 1));
    if (!file || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_stream_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto handle = static_cast<file_descriptor_handle*>(lua_touserdata(L, 2));
    if (!handle || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    if (*handle == INVALID_FILE_DESCRIPTOR) {
        push(L, std::errc::device_or_resource_busy);
        return lua_error(L);
    }

    lua_pushnil(L);
    setmetatable(L, 2);

    boost::system::error_code ec;
    file->assign(*handle, ec);
    assert(!ec); boost::ignore_unused(ec);

    return 0;
}

static int stream_release(lua_State* L)
{
    auto file = static_cast<asio::stream_file*>(lua_touserdata(L, 1));
    if (!file || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_stream_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    if (file->native_handle() == INVALID_FILE_DESCRIPTOR) {
        push(L, std::errc::bad_file_descriptor);
        return lua_error(L);
    }

    boost::system::error_code ec;
    int rawfd = file->release(ec);
    BOOST_SCOPE_EXIT_ALL(&) {
        if (rawfd != INVALID_FILE_DESCRIPTOR) {
            int res = close(rawfd);
            boost::ignore_unused(res);
        }
    };

    if (ec) {
        push(L, ec);
        return lua_error(L);
    }

    auto fdhandle = static_cast<file_descriptor_handle*>(
        lua_newuserdata(L, sizeof(file_descriptor_handle))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
    setmetatable(L, -2);

    *fdhandle = rawfd;
    rawfd = INVALID_FILE_DESCRIPTOR;
    return 1;
}
#endif // BOOST_OS_UNIX

static int stream_resize(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TNUMBER);

    auto file = static_cast<asio::stream_file*>(lua_touserdata(L, 1));
    if (!file || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_stream_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    file->resize(lua_tointeger(L, 2), ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int stream_seek(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TNUMBER);
    luaL_checktype(L, 3, LUA_TSTRING);

    auto file = static_cast<asio::stream_file*>(lua_touserdata(L, 1));
    if (!file || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_stream_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    asio::file_base::seek_basis whence;
    auto whence_str = tostringview(L, 3);
    if (whence_str == "set") {
        whence = asio::file_base::seek_set;
    } else if (whence_str == "cur") {
        whence = asio::file_base::seek_cur;
    } else if (whence_str == "end") {
        whence = asio::file_base::seek_end;
    } else {
        push(L, std::errc::invalid_argument);
        return lua_error(L);
    }

    boost::system::error_code ec;
    auto ret = file->seek(lua_tointeger(L, 2), whence, ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    lua_pushnumber(L, ret);
    return 1;
}

static int stream_read_some(lua_State* L)
{
    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto file = static_cast<asio::stream_file*>(lua_touserdata(L, 1));
    if (!file || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_stream_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto bs = static_cast<byte_span_handle*>(lua_touserdata(L, 2));
    if (!bs || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &byte_span_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    auto cancel_slot = set_default_interrupter(L, *vm_ctx);

    file->async_read_some(
        asio::buffer(bs->data.get(), bs->size),
        asio::bind_cancellation_slot(cancel_slot, asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,buf=bs->data](
                const boost::system::error_code& ec,
                std::size_t bytes_transferred
            ) {
                boost::ignore_unused(buf);
                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(ec, bytes_transferred)))
                );
            }
        ))
    );

    return lua_yield(L, 0);
}

static int stream_write_some(lua_State* L)
{
    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto file = static_cast<asio::stream_file*>(lua_touserdata(L, 1));
    if (!file || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_stream_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto bs = static_cast<byte_span_handle*>(lua_touserdata(L, 2));
    if (!bs || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &byte_span_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    auto cancel_slot = set_default_interrupter(L, *vm_ctx);

    file->async_write_some(
        asio::buffer(bs->data.get(), bs->size),
        asio::bind_cancellation_slot(cancel_slot, asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,buf=bs->data](
                const boost::system::error_code& ec,
                std::size_t bytes_transferred
            ) {
                boost::ignore_unused(buf);
                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(ec, bytes_transferred)))
                );
            }
        ))
    );

    return lua_yield(L, 0);
}

inline int stream_is_open(lua_State* L)
{
    auto file = static_cast<asio::stream_file*>(lua_touserdata(L, 1));
    lua_pushboolean(L, file->is_open());
    return 1;
}

inline int stream_size(lua_State* L)
{
    auto file = static_cast<asio::stream_file*>(lua_touserdata(L, 1));
    boost::system::error_code ec;
    auto ret = file->size(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    lua_pushnumber(L, ret);
    return 1;
}

static int stream_mt_index(lua_State* L)
{
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("open"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, stream_open);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("close"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, stream_close);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("cancel"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, stream_cancel);
                    return 1;
                }
            ),
#if BOOST_OS_UNIX
            hana::make_pair(
                BOOST_HANA_STRING("assign"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, stream_assign);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("release"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, stream_release);
                    return 1;
                }
            ),
#endif // BOOST_OS_UNIX
            hana::make_pair(
                BOOST_HANA_STRING("resize"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, stream_resize);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("seek"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, stream_seek);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("read_some"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX, &stream_read_some_key);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("write_some"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX, &stream_write_some_key);
                    return 1;
                }
            ),
            hana::make_pair(BOOST_HANA_STRING("is_open"), stream_is_open),
            hana::make_pair(BOOST_HANA_STRING("size"), stream_size)
        ),
        [](std::string_view /*key*/, lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        },
        tostringview(L, 2),
        L
    );
}

static int stream_new(lua_State* L)
{
    int nargs = lua_gettop(L);
    auto& vm_ctx = get_vm_context(L);

    if (nargs == 0) {
        auto file = static_cast<asio::stream_file*>(
            lua_newuserdata(L, sizeof(asio::stream_file))
        );
        rawgetp(L, LUA_REGISTRYINDEX, &file_stream_mt_key);
        setmetatable(L, -2);
        new (file) asio::stream_file{vm_ctx.strand().context()};
        return 1;
    }

#if BOOST_OS_UNIX
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

    auto file = static_cast<asio::stream_file*>(
        lua_newuserdata(L, sizeof(asio::stream_file))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &file_stream_mt_key);
    setmetatable(L, -2);
    new (file) asio::stream_file{vm_ctx.strand().context()};

    lua_pushnil(L);
    setmetatable(L, 1);

    boost::system::error_code ec;
    file->assign(*handle, ec);
    assert(!ec); boost::ignore_unused(ec);

    return 1;
#else // BOOST_OS_UNIX
    push(L, std::errc::invalid_argument, "arg", 1);
    return lua_error(L);
#endif // BOOST_OS_UNIX
}

static int random_access_open(lua_State* L)
{
    luaL_checktype(L, 3, LUA_TNUMBER);

    auto file = static_cast<asio::random_access_file*>(lua_touserdata(L, 1));
    if (!file || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_random_access_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    std::string path;
    try {
        auto p = static_cast<std::filesystem::path*>(lua_touserdata(L, 2));
        if (!p || !lua_getmetatable(L, 2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        path = p->string();
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }

    auto flags = static_cast<asio::file_base::flags>(lua_tointeger(L, 3));

    boost::system::error_code ec;
    file->open(path, flags, ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int random_access_close(lua_State* L)
{
    auto file = static_cast<asio::random_access_file*>(lua_touserdata(L, 1));
    if (!file || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_random_access_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    file->close(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int random_access_cancel(lua_State* L)
{
    auto file = static_cast<asio::random_access_file*>(lua_touserdata(L, 1));
    if (!file || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_random_access_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    file->cancel(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

#if BOOST_OS_UNIX
static int random_access_assign(lua_State* L)
{
    auto file = static_cast<asio::random_access_file*>(lua_touserdata(L, 1));
    if (!file || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_random_access_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto handle = static_cast<file_descriptor_handle*>(lua_touserdata(L, 2));
    if (!handle || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    if (*handle == INVALID_FILE_DESCRIPTOR) {
        push(L, std::errc::device_or_resource_busy);
        return lua_error(L);
    }

    lua_pushnil(L);
    setmetatable(L, 2);

    boost::system::error_code ec;
    file->assign(*handle, ec);
    assert(!ec); boost::ignore_unused(ec);

    return 0;
}

static int random_access_release(lua_State* L)
{
    auto file = static_cast<asio::random_access_file*>(lua_touserdata(L, 1));
    if (!file || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_random_access_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    if (file->native_handle() == INVALID_FILE_DESCRIPTOR) {
        push(L, std::errc::bad_file_descriptor);
        return lua_error(L);
    }

    boost::system::error_code ec;
    int rawfd = file->release(ec);
    BOOST_SCOPE_EXIT_ALL(&) {
        if (rawfd != INVALID_FILE_DESCRIPTOR) {
            int res = close(rawfd);
            boost::ignore_unused(res);
        }
    };

    if (ec) {
        push(L, ec);
        return lua_error(L);
    }

    auto fdhandle = static_cast<file_descriptor_handle*>(
        lua_newuserdata(L, sizeof(file_descriptor_handle))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
    setmetatable(L, -2);

    *fdhandle = rawfd;
    rawfd = INVALID_FILE_DESCRIPTOR;
    return 1;
}
#endif // BOOST_OS_UNIX

static int random_access_resize(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TNUMBER);

    auto file = static_cast<asio::random_access_file*>(lua_touserdata(L, 1));
    if (!file || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_random_access_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    file->resize(lua_tointeger(L, 2), ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int random_access_read_some_at(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TNUMBER);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto file = static_cast<asio::random_access_file*>(lua_touserdata(L, 1));
    if (!file || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_random_access_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto bs = static_cast<byte_span_handle*>(lua_touserdata(L, 3));
    if (!bs || !lua_getmetatable(L, 3)) {
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &byte_span_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    }

    auto cancel_slot = set_default_interrupter(L, *vm_ctx);

    file->async_read_some_at(
        lua_tointeger(L, 2),
        asio::buffer(bs->data.get(), bs->size),
        asio::bind_cancellation_slot(cancel_slot, asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,buf=bs->data](
                const boost::system::error_code& ec,
                std::size_t bytes_transferred
            ) {
                boost::ignore_unused(buf);
                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(ec, bytes_transferred)))
                );
            }
        ))
    );

    return lua_yield(L, 0);
}

static int random_access_write_some_at(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TNUMBER);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto file = static_cast<asio::random_access_file*>(lua_touserdata(L, 1));
    if (!file || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_random_access_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto bs = static_cast<byte_span_handle*>(lua_touserdata(L, 3));
    if (!bs || !lua_getmetatable(L, 3)) {
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &byte_span_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    }

    auto cancel_slot = set_default_interrupter(L, *vm_ctx);

    file->async_write_some_at(
        lua_tointeger(L, 2),
        asio::buffer(bs->data.get(), bs->size),
        asio::bind_cancellation_slot(cancel_slot, asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,buf=bs->data](
                const boost::system::error_code& ec,
                std::size_t bytes_transferred
            ) {
                boost::ignore_unused(buf);
                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(ec, bytes_transferred)))
                );
            }
        ))
    );

    return lua_yield(L, 0);
}

inline int random_access_is_open(lua_State* L)
{
    auto file = static_cast<asio::random_access_file*>(lua_touserdata(L, 1));
    lua_pushboolean(L, file->is_open());
    return 1;
}

inline int random_access_size(lua_State* L)
{
    auto file = static_cast<asio::random_access_file*>(lua_touserdata(L, 1));
    boost::system::error_code ec;
    auto ret = file->size(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    lua_pushnumber(L, ret);
    return 1;
}

static int random_access_mt_index(lua_State* L)
{
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("open"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, random_access_open);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("close"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, random_access_close);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("cancel"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, random_access_cancel);
                    return 1;
                }
            ),
#if BOOST_OS_UNIX
            hana::make_pair(
                BOOST_HANA_STRING("assign"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, random_access_assign);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("release"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, random_access_release);
                    return 1;
                }
            ),
#endif // BOOST_OS_UNIX
            hana::make_pair(
                BOOST_HANA_STRING("resize"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, random_access_resize);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("read_some_at"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX,
                            &random_access_read_some_at_key);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("write_some_at"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX,
                            &random_access_write_some_at_key);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("is_open"), random_access_is_open),
            hana::make_pair(
                BOOST_HANA_STRING("size"), random_access_size)
        ),
        [](std::string_view /*key*/, lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        },
        tostringview(L, 2),
        L
    );
}

static int random_access_new(lua_State* L)
{
    int nargs = lua_gettop(L);
    auto& vm_ctx = get_vm_context(L);

    if (nargs == 0) {
        auto file = static_cast<asio::random_access_file*>(
            lua_newuserdata(L, sizeof(asio::random_access_file))
        );
        rawgetp(L, LUA_REGISTRYINDEX, &file_random_access_mt_key);
        setmetatable(L, -2);
        new (file) asio::random_access_file{vm_ctx.strand().context()};
        return 1;
    }

#if BOOST_OS_UNIX
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

    auto file = static_cast<asio::random_access_file*>(
        lua_newuserdata(L, sizeof(asio::random_access_file))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &file_random_access_mt_key);
    setmetatable(L, -2);
    new (file) asio::random_access_file{vm_ctx.strand().context()};

    lua_pushnil(L);
    setmetatable(L, 1);

    boost::system::error_code ec;
    file->assign(*handle, ec);
    assert(!ec); boost::ignore_unused(ec);

    return 1;
#else // BOOST_OS_UNIX
    push(L, std::errc::invalid_argument, "arg", 1);
    return lua_error(L);
#endif // BOOST_OS_UNIX
}

void init_file(lua_State* L)
{
    lua_pushlightuserdata(L, &file_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/5);

        lua_pushliteral(L, "open_flag");
        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/8);

            lua_pushliteral(L, "append");
            lua_pushinteger(L, asio::file_base::append);
            lua_rawset(L, -3);

            lua_pushliteral(L, "create");
            lua_pushinteger(L, asio::file_base::create);
            lua_rawset(L, -3);

            lua_pushliteral(L, "exclusive");
            lua_pushinteger(L, asio::file_base::exclusive);
            lua_rawset(L, -3);

            lua_pushliteral(L, "read_only");
            lua_pushinteger(L, asio::file_base::read_only);
            lua_rawset(L, -3);

            lua_pushliteral(L, "read_write");
            lua_pushinteger(L, asio::file_base::read_write);
            lua_rawset(L, -3);

            lua_pushliteral(L, "sync_all_on_write");
            lua_pushinteger(L, asio::file_base::sync_all_on_write);
            lua_rawset(L, -3);

            lua_pushliteral(L, "truncate");
            lua_pushinteger(L, asio::file_base::truncate);
            lua_rawset(L, -3);

            lua_pushliteral(L, "write_only");
            lua_pushinteger(L, asio::file_base::write_only);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);

        lua_pushliteral(L, "write_all_at");
        {
            int res = luaL_loadbuffer(
                L, reinterpret_cast<char*>(write_all_at_bytecode),
                write_all_at_bytecode_size, nullptr);
            assert(res == 0); boost::ignore_unused(res);
            rawgetp(L, LUA_REGISTRYINDEX, &raw_type_key);
            lua_pushcfunction(L, byte_span_non_member_append);
            lua_call(L, 2, 1);
        }
        lua_rawset(L, -3);

        lua_pushliteral(L, "write_at_least_at");
        int res = luaL_loadbuffer(
            L, reinterpret_cast<char*>(write_at_least_at_bytecode),
            write_at_least_at_bytecode_size, nullptr);
        assert(res == 0); boost::ignore_unused(res);
        lua_rawset(L, -3);

        lua_pushliteral(L, "read_all_at");
        res = luaL_loadbuffer(
            L, reinterpret_cast<char*>(read_all_at_bytecode),
            read_all_at_bytecode_size, nullptr);
        assert(res == 0); boost::ignore_unused(res);
        lua_rawset(L, -3);

        lua_pushliteral(L, "read_at_least_at");
        res = luaL_loadbuffer(
            L, reinterpret_cast<char*>(read_at_least_at_bytecode),
            read_at_least_at_bytecode_size, nullptr);
        assert(res == 0); boost::ignore_unused(res);
        lua_rawset(L, -3);

        lua_pushliteral(L, "stream");
        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/1);

            lua_pushliteral(L, "new");
            lua_pushcfunction(L, stream_new);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);

        lua_pushliteral(L, "random_access");
        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/1);

            lua_pushliteral(L, "new");
            lua_pushcfunction(L, random_access_new);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &file_stream_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "file.stream");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, stream_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<asio::stream_file>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &file_random_access_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "file.random_access");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, random_access_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<asio::random_access_file>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &stream_read_some_key);
    rawgetp(L, LUA_REGISTRYINDEX,
            &var_args__retval1_to_error__fwd_retval2__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, stream_read_some);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &stream_write_some_key);
    rawgetp(L, LUA_REGISTRYINDEX,
            &var_args__retval1_to_error__fwd_retval2__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, stream_write_some);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &random_access_read_some_at_key);
    rawgetp(L, LUA_REGISTRYINDEX,
            &var_args__retval1_to_error__fwd_retval2__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, random_access_read_some_at);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &random_access_write_some_at_key);
    rawgetp(L, LUA_REGISTRYINDEX,
            &var_args__retval1_to_error__fwd_retval2__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, random_access_write_some_at);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);
}

} // namespace emilua
