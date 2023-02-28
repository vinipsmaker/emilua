/* Copyright (c) 2023 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#include <emilua/generic_error.hpp>

#include <emilua/dispatch_table.hpp>

namespace emilua {

char generic_error_key;

int posix_errno_code_from_name(std::string_view name)
{
    return static_cast<int>(dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("EAFNOSUPPORT"),
                []() { return std::errc::address_family_not_supported; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("EADDRINUSE"),
                []() { return std::errc::address_in_use; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("EADDRNOTAVAIL"),
                []() { return std::errc::address_not_available; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("EISCONN"),
                []() { return std::errc::already_connected; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("E2BIG"),
                []() { return std::errc::argument_list_too_long; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("EDOM"),
                []() { return std::errc::argument_out_of_domain; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("EFAULT"),
                []() { return std::errc::bad_address; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("EBADF"),
                []() { return std::errc::bad_file_descriptor; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("EBADMSG"),
                []() { return std::errc::bad_message; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("EPIPE"),
                []() { return std::errc::broken_pipe; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("ECONNABORTED"),
                []() { return std::errc::connection_aborted; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("EALREADY"),
                []() { return std::errc::connection_already_in_progress; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("ECONNREFUSED"),
                []() { return std::errc::connection_refused; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("ECONNRESET"),
                []() { return std::errc::connection_reset; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("EXDEV"),
                []() { return std::errc::cross_device_link; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("EDESTADDRREQ"),
                []() { return std::errc::destination_address_required; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("EBUSY"),
                []() { return std::errc::device_or_resource_busy; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("ENOTEMPTY"),
                []() { return std::errc::directory_not_empty; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("ENOEXEC"),
                []() { return std::errc::executable_format_error; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("EEXIST"),
                []() { return std::errc::file_exists; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("EFBIG"),
                []() { return std::errc::file_too_large; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("ENAMETOOLONG"),
                []() { return std::errc::filename_too_long; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("ENOSYS"),
                []() { return std::errc::function_not_supported; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("EHOSTUNREACH"),
                []() { return std::errc::host_unreachable; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("EIDRM"),
                []() { return std::errc::identifier_removed; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("EILSEQ"),
                []() { return std::errc::illegal_byte_sequence; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("ENOTTY"),
                []() { return std::errc::inappropriate_io_control_operation; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("EINTR"),
                []() { return std::errc::interrupted; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("EINVAL"),
                []() { return std::errc::invalid_argument; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("ESPIPE"),
                []() { return std::errc::invalid_seek; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("EIO"),
                []() { return std::errc::io_error; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("EISDIR"),
                []() { return std::errc::is_a_directory; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("EMSGSIZE"),
                []() { return std::errc::message_size; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("ENETDOWN"),
                []() { return std::errc::network_down; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("ENETRESET"),
                []() { return std::errc::network_reset; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("ENETUNREACH"),
                []() { return std::errc::network_unreachable; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("ENOBUFS"),
                []() { return std::errc::no_buffer_space; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("ECHILD"),
                []() { return std::errc::no_child_process; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("ENOLINK"),
                []() { return std::errc::no_link; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("ENOLCK"),
                []() { return std::errc::no_lock_available; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("ENOMSG"),
                []() { return std::errc::no_message; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("ENOPROTOOPT"),
                []() { return std::errc::no_protocol_option; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("ENOSPC"),
                []() { return std::errc::no_space_on_device; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("ENXIO"),
                []() { return std::errc::no_such_device_or_address; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("ENODEV"),
                []() { return std::errc::no_such_device; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("ENOENT"),
                []() { return std::errc::no_such_file_or_directory; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("ESRCH"),
                []() { return std::errc::no_such_process; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("ENOTDIR"),
                []() { return std::errc::not_a_directory; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("ENOTSOCK"),
                []() { return std::errc::not_a_socket; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("ENOTCONN"),
                []() { return std::errc::not_connected; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("ENOMEM"),
                []() { return std::errc::not_enough_memory; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("ENOTSUP"),
                []() { return std::errc::not_supported; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("ECANCELED"),
                []() { return std::errc::operation_canceled; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("EINPROGRESS"),
                []() { return std::errc::operation_in_progress; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("EPERM"),
                []() { return std::errc::operation_not_permitted; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("EOPNOTSUPP"),
                []() { return std::errc::operation_not_supported; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("EWOULDBLOCK"),
                []() { return std::errc::operation_would_block; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("EOWNERDEAD"),
                []() { return std::errc::owner_dead; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("EACCES"),
                []() { return std::errc::permission_denied; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("EPROTO"),
                []() { return std::errc::protocol_error; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("EPROTONOSUPPORT"),
                []() { return std::errc::protocol_not_supported; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("EROFS"),
                []() { return std::errc::read_only_file_system; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("EDEADLK"),
                []() { return std::errc::resource_deadlock_would_occur; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("EAGAIN"),
                []() { return std::errc::resource_unavailable_try_again; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("ERANGE"),
                []() { return std::errc::result_out_of_range; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("ENOTRECOVERABLE"),
                []() { return std::errc::state_not_recoverable; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("ETXTBSY"),
                []() { return std::errc::text_file_busy; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("ETIMEDOUT"),
                []() { return std::errc::timed_out; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("ENFILE"),
                []() { return std::errc::too_many_files_open_in_system; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("EMFILE"),
                []() { return std::errc::too_many_files_open; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("EMLINK"),
                []() { return std::errc::too_many_links; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("ELOOP"),
                []() { return std::errc::too_many_symbolic_link_levels; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("EOVERFLOW"),
                []() { return std::errc::value_too_large; }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("EPROTOTYPE"),
                []() { return std::errc::wrong_protocol_type; }
            )
        ),
        [](std::string_view /*key*/) -> std::errc {
            return {};
        },
        name
    ));
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
