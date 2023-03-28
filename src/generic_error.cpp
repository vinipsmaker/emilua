/* Copyright (c) 2023 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

EMILUA_GPERF_DECLS_BEGIN(includes)
#include <emilua/generic_error.hpp>
EMILUA_GPERF_DECLS_END(includes)

namespace emilua {

char generic_error_key;

int posix_errno_code_from_name(std::string_view name)
{
    return static_cast<int>(EMILUA_GPERF_BEGIN(name)
        EMILUA_GPERF_PARAM(std::errc action)
        EMILUA_GPERF_DEFAULT_VALUE(std::errc{})
        EMILUA_GPERF_PAIR(
            "EAFNOSUPPORT", std::errc::address_family_not_supported
        )
        EMILUA_GPERF_PAIR("EADDRINUSE", std::errc::address_in_use)
        EMILUA_GPERF_PAIR("EADDRNOTAVAIL", std::errc::address_not_available)
        EMILUA_GPERF_PAIR("EISCONN", std::errc::already_connected)
        EMILUA_GPERF_PAIR("E2BIG", std::errc::argument_list_too_long)
        EMILUA_GPERF_PAIR("EDOM", std::errc::argument_out_of_domain)
        EMILUA_GPERF_PAIR("EFAULT", std::errc::bad_address)
        EMILUA_GPERF_PAIR("EBADF", std::errc::bad_file_descriptor)
        EMILUA_GPERF_PAIR("EBADMSG", std::errc::bad_message)
        EMILUA_GPERF_PAIR("EPIPE", std::errc::broken_pipe)
        EMILUA_GPERF_PAIR("ECONNABORTED", std::errc::connection_aborted)
        EMILUA_GPERF_PAIR("EALREADY", std::errc::connection_already_in_progress)
        EMILUA_GPERF_PAIR("ECONNREFUSED", std::errc::connection_refused)
        EMILUA_GPERF_PAIR("ECONNRESET", std::errc::connection_reset)
        EMILUA_GPERF_PAIR("EXDEV", std::errc::cross_device_link)
        EMILUA_GPERF_PAIR(
            "EDESTADDRREQ", std::errc::destination_address_required
        )
        EMILUA_GPERF_PAIR("EBUSY", std::errc::device_or_resource_busy)
        EMILUA_GPERF_PAIR("ENOTEMPTY", std::errc::directory_not_empty)
        EMILUA_GPERF_PAIR("ENOEXEC", std::errc::executable_format_error)
        EMILUA_GPERF_PAIR("EEXIST", std::errc::file_exists)
        EMILUA_GPERF_PAIR("EFBIG", std::errc::file_too_large)
        EMILUA_GPERF_PAIR("ENAMETOOLONG", std::errc::filename_too_long)
        EMILUA_GPERF_PAIR("ENOSYS", std::errc::function_not_supported)
        EMILUA_GPERF_PAIR("EHOSTUNREACH", std::errc::host_unreachable)
        EMILUA_GPERF_PAIR("EIDRM", std::errc::identifier_removed)
        EMILUA_GPERF_PAIR("EILSEQ", std::errc::illegal_byte_sequence)
        EMILUA_GPERF_PAIR(
            "ENOTTY", std::errc::inappropriate_io_control_operation
        )
        EMILUA_GPERF_PAIR("EINTR", std::errc::interrupted)
        EMILUA_GPERF_PAIR("EINVAL", std::errc::invalid_argument)
        EMILUA_GPERF_PAIR("ESPIPE", std::errc::invalid_seek)
        EMILUA_GPERF_PAIR("EIO", std::errc::io_error)
        EMILUA_GPERF_PAIR("EISDIR", std::errc::is_a_directory)
        EMILUA_GPERF_PAIR("EMSGSIZE", std::errc::message_size)
        EMILUA_GPERF_PAIR("ENETDOWN", std::errc::network_down)
        EMILUA_GPERF_PAIR("ENETRESET", std::errc::network_reset)
        EMILUA_GPERF_PAIR("ENETUNREACH", std::errc::network_unreachable)
        EMILUA_GPERF_PAIR("ENOBUFS", std::errc::no_buffer_space)
        EMILUA_GPERF_PAIR("ECHILD", std::errc::no_child_process)
        EMILUA_GPERF_PAIR("ENOLINK", std::errc::no_link)
        EMILUA_GPERF_PAIR("ENOLCK", std::errc::no_lock_available)
        EMILUA_GPERF_PAIR("ENOMSG", std::errc::no_message)
        EMILUA_GPERF_PAIR("ENOPROTOOPT", std::errc::no_protocol_option)
        EMILUA_GPERF_PAIR("ENOSPC", std::errc::no_space_on_device)
        EMILUA_GPERF_PAIR("ENXIO", std::errc::no_such_device_or_address)
        EMILUA_GPERF_PAIR("ENODEV", std::errc::no_such_device)
        EMILUA_GPERF_PAIR("ENOENT", std::errc::no_such_file_or_directory)
        EMILUA_GPERF_PAIR("ESRCH", std::errc::no_such_process)
        EMILUA_GPERF_PAIR("ENOTDIR", std::errc::not_a_directory)
        EMILUA_GPERF_PAIR("ENOTSOCK", std::errc::not_a_socket)
        EMILUA_GPERF_PAIR("ENOTCONN", std::errc::not_connected)
        EMILUA_GPERF_PAIR("ENOMEM", std::errc::not_enough_memory)
        EMILUA_GPERF_PAIR("ENOTSUP", std::errc::not_supported)
        EMILUA_GPERF_PAIR("ECANCELED", std::errc::operation_canceled)
        EMILUA_GPERF_PAIR("EINPROGRESS", std::errc::operation_in_progress)
        EMILUA_GPERF_PAIR("EPERM", std::errc::operation_not_permitted)
        EMILUA_GPERF_PAIR("EOPNOTSUPP", std::errc::operation_not_supported)
        EMILUA_GPERF_PAIR("EWOULDBLOCK", std::errc::operation_would_block)
        EMILUA_GPERF_PAIR("EOWNERDEAD", std::errc::owner_dead)
        EMILUA_GPERF_PAIR("EACCES", std::errc::permission_denied)
        EMILUA_GPERF_PAIR("EPROTO", std::errc::protocol_error)
        EMILUA_GPERF_PAIR("EPROTONOSUPPORT", std::errc::protocol_not_supported)
        EMILUA_GPERF_PAIR("EROFS", std::errc::read_only_file_system)
        EMILUA_GPERF_PAIR("EDEADLK", std::errc::resource_deadlock_would_occur)
        EMILUA_GPERF_PAIR("EAGAIN", std::errc::resource_unavailable_try_again)
        EMILUA_GPERF_PAIR("ERANGE", std::errc::result_out_of_range)
        EMILUA_GPERF_PAIR("ENOTRECOVERABLE", std::errc::state_not_recoverable)
        EMILUA_GPERF_PAIR("ETXTBSY", std::errc::text_file_busy)
        EMILUA_GPERF_PAIR("ETIMEDOUT", std::errc::timed_out)
        EMILUA_GPERF_PAIR("ENFILE", std::errc::too_many_files_open_in_system)
        EMILUA_GPERF_PAIR("EMFILE", std::errc::too_many_files_open)
        EMILUA_GPERF_PAIR("EMLINK", std::errc::too_many_links)
        EMILUA_GPERF_PAIR("ELOOP", std::errc::too_many_symbolic_link_levels)
        EMILUA_GPERF_PAIR("EOVERFLOW", std::errc::value_too_large)
        EMILUA_GPERF_PAIR("EPROTOTYPE", std::errc::wrong_protocol_type)
    EMILUA_GPERF_END(name));
}

static int generic_error_mt_index(lua_State* L)
{
    int ge = posix_errno_code_from_name(tostringview(L, 2));
    if (ge == 0) {
        push(L, errc::bad_index, "index", 2);
        return lua_error(L);
    }

    push(L, std::error_code{ge, std::generic_category()});
    return 1;
}

void init_generic_error(lua_State* L)
{
    lua_pushlightuserdata(L, &generic_error_key);
    lua_newuserdata(L, sizeof(char));
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/2);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "generic_error");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, generic_error_mt_index);
        lua_rawset(L, -3);
    }
    setmetatable(L, -2);
    lua_rawset(L, LUA_REGISTRYINDEX);
}

} // namespace emilua
