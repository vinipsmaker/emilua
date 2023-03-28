/* Copyright (c) 2022 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

EMILUA_GPERF_DECLS_BEGIN(includes)
#include <boost/asio/readable_pipe.hpp>
#include <boost/asio/writable_pipe.hpp>
#include <boost/asio/connect_pipe.hpp>
#include <boost/scope_exit.hpp>

#include <emilua/file_descriptor.hpp>
#include <emilua/async_base.hpp>
#include <emilua/byte_span.hpp>
#include <emilua/pipe.hpp>
EMILUA_GPERF_DECLS_END(includes)

namespace emilua {

char pipe_key;
char readable_pipe_mt_key;
char writable_pipe_mt_key;

EMILUA_GPERF_DECLS_BEGIN(pipe)
EMILUA_GPERF_NAMESPACE(emilua)
static char readable_pipe_read_some_key;
static char writable_pipe_write_some_key;
EMILUA_GPERF_DECLS_END(pipe)

EMILUA_GPERF_DECLS_BEGIN(read_stream)
EMILUA_GPERF_NAMESPACE(emilua)
static int readable_pipe_close(lua_State* L)
{
    auto pipe = static_cast<asio::readable_pipe*>(lua_touserdata(L, 1));
    if (!pipe || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &readable_pipe_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    pipe->close(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int readable_pipe_cancel(lua_State* L)
{
    auto pipe = static_cast<asio::readable_pipe*>(lua_touserdata(L, 1));
    if (!pipe || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &readable_pipe_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    pipe->cancel(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

#if BOOST_OS_UNIX
static int readable_pipe_assign(lua_State* L)
{
    auto pipe = static_cast<asio::readable_pipe*>(lua_touserdata(L, 1));
    if (!pipe || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &readable_pipe_mt_key);
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
    pipe->assign(*handle, ec);
    assert(!ec); boost::ignore_unused(ec);

    return 0;
}

static int readable_pipe_release(lua_State* L)
{
    auto pipe = static_cast<asio::readable_pipe*>(lua_touserdata(L, 1));
    if (!pipe || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &readable_pipe_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    if (pipe->native_handle() == INVALID_FILE_DESCRIPTOR) {
        push(L, std::errc::bad_file_descriptor);
        return lua_error(L);
    }

    // https://github.com/chriskohlhoff/asio/issues/1043
    int newfd = dup(pipe->native_handle());
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

    boost::system::error_code ignored_ec;
    pipe->close(ignored_ec);

    auto fdhandle = static_cast<file_descriptor_handle*>(
        lua_newuserdata(L, sizeof(file_descriptor_handle))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
    setmetatable(L, -2);

    *fdhandle = newfd;
    newfd = -1;
    return 1;
}
#endif // BOOST_OS_UNIX
EMILUA_GPERF_DECLS_END(read_stream)

static int readable_pipe_read_some(lua_State* L)
{
    lua_settop(L, 2);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto pipe = static_cast<asio::readable_pipe*>(lua_touserdata(L, 1));
    if (!pipe || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &readable_pipe_mt_key);
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

    lua_pushvalue(L, 1);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto pipe = static_cast<asio::readable_pipe*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            boost::system::error_code ignored_ec;
            pipe->cancel(ignored_ec);
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    pipe->async_read_some(
        asio::buffer(bs->data.get(), bs->size),
        asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,buf=bs->data](
                const boost::system::error_code& ec,
                std::size_t bytes_transferred
            ) {
                boost::ignore_unused(buf);
                auto opt_args = vm_context::options::arguments;
                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            opt_args,
                            hana::make_tuple(ec, bytes_transferred))));
            }
        )
    );

    return lua_yield(L, 0);
}

EMILUA_GPERF_DECLS_BEGIN(read_stream)
EMILUA_GPERF_NAMESPACE(emilua)
inline int readable_pipe_is_open(lua_State* L)
{
    auto pipe = static_cast<asio::readable_pipe*>(lua_touserdata(L, 1));
    lua_pushboolean(L, pipe->is_open());
    return 1;
}
EMILUA_GPERF_DECLS_END(read_stream)

static int readable_pipe_mt_index(lua_State* L)
{
    auto key = tostringview(L, 2);
    return EMILUA_GPERF_BEGIN(key)
        EMILUA_GPERF_PARAM(int (*action)(lua_State*))
        EMILUA_GPERF_DEFAULT_VALUE([](lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        })
        EMILUA_GPERF_PAIR(
            "close",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, readable_pipe_close);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "cancel",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, readable_pipe_cancel);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "assign",
            [](lua_State* L) -> int {
#if BOOST_OS_UNIX
                lua_pushcfunction(L, readable_pipe_assign);
#else // BOOST_OS_UNIX
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_UNIX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "release",
            [](lua_State* L) -> int {
#if BOOST_OS_UNIX
                lua_pushcfunction(L, readable_pipe_release);
#else // BOOST_OS_UNIX
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_UNIX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "read_some",
            [](lua_State* L) -> int {
                rawgetp(L, LUA_REGISTRYINDEX, &readable_pipe_read_some_key);
                return 1;
            })
        EMILUA_GPERF_PAIR("is_open", readable_pipe_is_open)
    EMILUA_GPERF_END(key)(L);
}

static int readable_pipe_new(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);

#if BOOST_OS_UNIX
    int nargs = lua_gettop(L);
    if (nargs == 0) {
#endif // BOOST_OS_UNIX

        auto pipe = static_cast<asio::readable_pipe*>(
            lua_newuserdata(L, sizeof(asio::readable_pipe))
        );
        rawgetp(L, LUA_REGISTRYINDEX, &readable_pipe_mt_key);
        setmetatable(L, -2);
        new (pipe) asio::readable_pipe{vm_ctx.strand().context()};
        return 1;

#if BOOST_OS_UNIX
    }

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

    auto pipe = static_cast<asio::readable_pipe*>(
        lua_newuserdata(L, sizeof(asio::readable_pipe))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &readable_pipe_mt_key);
    setmetatable(L, -2);
    new (pipe) asio::readable_pipe{vm_ctx.strand().context()};

    lua_pushnil(L);
    setmetatable(L, 1);

    boost::system::error_code ec;
    pipe->assign(*handle, ec);
    assert(!ec); boost::ignore_unused(ec);

    return 1;
#endif // BOOST_OS_UNIX
}

EMILUA_GPERF_DECLS_BEGIN(write_stream)
EMILUA_GPERF_NAMESPACE(emilua)
static int writable_pipe_close(lua_State* L)
{
    auto pipe = static_cast<asio::writable_pipe*>(lua_touserdata(L, 1));
    if (!pipe || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &writable_pipe_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    pipe->close(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int writable_pipe_cancel(lua_State* L)
{
    auto pipe = static_cast<asio::writable_pipe*>(lua_touserdata(L, 1));
    if (!pipe || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &writable_pipe_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    pipe->cancel(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

#if BOOST_OS_UNIX
static int writable_pipe_assign(lua_State* L)
{
    auto pipe = static_cast<asio::writable_pipe*>(lua_touserdata(L, 1));
    if (!pipe || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &writable_pipe_mt_key);
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
    pipe->assign(*handle, ec);
    assert(!ec); boost::ignore_unused(ec);

    return 0;
}

static int writable_pipe_release(lua_State* L)
{
    auto pipe = static_cast<asio::writable_pipe*>(lua_touserdata(L, 1));
    if (!pipe || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &writable_pipe_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    if (pipe->native_handle() == INVALID_FILE_DESCRIPTOR) {
        push(L, std::errc::bad_file_descriptor);
        return lua_error(L);
    }

    // https://github.com/chriskohlhoff/asio/issues/1043
    int newfd = dup(pipe->native_handle());
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

    boost::system::error_code ignored_ec;
    pipe->close(ignored_ec);

    auto fdhandle = static_cast<file_descriptor_handle*>(
        lua_newuserdata(L, sizeof(file_descriptor_handle))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
    setmetatable(L, -2);

    *fdhandle = newfd;
    newfd = -1;
    return 1;
}
#endif // BOOST_OS_UNIX
EMILUA_GPERF_DECLS_END(write_stream)

static int writable_pipe_write_some(lua_State* L)
{
    lua_settop(L, 2);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto pipe = static_cast<asio::writable_pipe*>(lua_touserdata(L, 1));
    if (!pipe || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &writable_pipe_mt_key);
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

    lua_pushvalue(L, 1);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto pipe = static_cast<asio::writable_pipe*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            boost::system::error_code ignored_ec;
            pipe->cancel(ignored_ec);
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    pipe->async_write_some(
        asio::buffer(bs->data.get(), bs->size),
        asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,buf=bs->data](
                const boost::system::error_code& ec,
                std::size_t bytes_transferred
            ) {
                boost::ignore_unused(buf);
                auto opt_args = vm_context::options::arguments;
                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            opt_args,
                            hana::make_tuple(ec, bytes_transferred))));
            }
        )
    );

    return lua_yield(L, 0);
}

EMILUA_GPERF_DECLS_BEGIN(write_stream)
EMILUA_GPERF_NAMESPACE(emilua)
inline int writable_pipe_is_open(lua_State* L)
{
    auto pipe = static_cast<asio::writable_pipe*>(lua_touserdata(L, 1));
    lua_pushboolean(L, pipe->is_open());
    return 1;
}
EMILUA_GPERF_DECLS_END(write_stream)

static int writable_pipe_mt_index(lua_State* L)
{
    auto key = tostringview(L, 2);
    return EMILUA_GPERF_BEGIN(key)
        EMILUA_GPERF_PARAM(int (*action)(lua_State*))
        EMILUA_GPERF_DEFAULT_VALUE([](lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        })
        EMILUA_GPERF_PAIR(
            "close",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, writable_pipe_close);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "cancel",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, writable_pipe_cancel);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "assign",
            [](lua_State* L) -> int {
#if BOOST_OS_UNIX
                lua_pushcfunction(L, writable_pipe_assign);
#else // BOOST_OS_UNIX
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_UNIX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "release",
            [](lua_State* L) -> int {
#if BOOST_OS_UNIX
                lua_pushcfunction(L, writable_pipe_release);
#else // BOOST_OS_UNIX
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_UNIX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "write_some",
            [](lua_State* L) -> int {
                rawgetp(L, LUA_REGISTRYINDEX, &writable_pipe_write_some_key);
                return 1;
            })
        EMILUA_GPERF_PAIR("is_open", writable_pipe_is_open)
    EMILUA_GPERF_END(key)(L);
}

static int writable_pipe_new(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);

#if BOOST_OS_UNIX
    int nargs = lua_gettop(L);
    if (nargs == 0) {
#endif // BOOST_OS_UNIX

        auto pipe = static_cast<asio::writable_pipe*>(
            lua_newuserdata(L, sizeof(asio::writable_pipe))
        );
        rawgetp(L, LUA_REGISTRYINDEX, &writable_pipe_mt_key);
        setmetatable(L, -2);
        new (pipe) asio::writable_pipe{vm_ctx.strand().context()};
        return 1;

#if BOOST_OS_UNIX
    }

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

    auto pipe = static_cast<asio::writable_pipe*>(
        lua_newuserdata(L, sizeof(asio::writable_pipe))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &writable_pipe_mt_key);
    setmetatable(L, -2);
    new (pipe) asio::writable_pipe{vm_ctx.strand().context()};

    lua_pushnil(L);
    setmetatable(L, 1);

    boost::system::error_code ec;
    pipe->assign(*handle, ec);
    assert(!ec); boost::ignore_unused(ec);

    return 1;
#endif // BOOST_OS_UNIX
}

static int pair(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);

    auto read_end = static_cast<asio::readable_pipe*>(
        lua_newuserdata(L, sizeof(asio::readable_pipe))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &readable_pipe_mt_key);
    setmetatable(L, -2);
    new (read_end) asio::readable_pipe{vm_ctx.strand().context()};

    auto write_end = static_cast<asio::writable_pipe*>(
        lua_newuserdata(L, sizeof(asio::writable_pipe))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &writable_pipe_mt_key);
    setmetatable(L, -2);
    new (write_end) asio::writable_pipe{vm_ctx.strand().context()};

    boost::system::error_code ec;
    asio::connect_pipe(*read_end, *write_end, ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 2;
}

void init_pipe(lua_State* L)
{
    lua_pushlightuserdata(L, &pipe_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "pair");
        lua_pushcfunction(L, pair);
        lua_rawset(L, -3);

        lua_pushliteral(L, "read_stream");
        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/1);

            lua_pushliteral(L, "new");
            lua_pushcfunction(L, readable_pipe_new);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);

        lua_pushliteral(L, "write_stream");
        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/1);

            lua_pushliteral(L, "new");
            lua_pushcfunction(L, writable_pipe_new);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &readable_pipe_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "pipe.read_stream");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, readable_pipe_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<asio::readable_pipe>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &writable_pipe_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "pipe.write_stream");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, writable_pipe_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<asio::writable_pipe>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &readable_pipe_read_some_key);
    rawgetp(L, LUA_REGISTRYINDEX,
            &var_args__retval1_to_error__fwd_retval2__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, readable_pipe_read_some);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &writable_pipe_write_some_key);
    rawgetp(L, LUA_REGISTRYINDEX,
            &var_args__retval1_to_error__fwd_retval2__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, writable_pipe_write_some);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);
}

} // namespace emilua
