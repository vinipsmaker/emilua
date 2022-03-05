/* Copyright (c) 2022 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#include <boost/asio/serial_port.hpp>

#include <emilua/dispatch_table.hpp>
#include <emilua/serial_port.hpp>
#include <emilua/async_base.hpp>
#include <emilua/byte_span.hpp>

namespace emilua {

char serial_port_key;
char serial_port_mt_key;

static char serial_port_read_some_key;
static char serial_port_write_some_key;

static int serial_port_open(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);

    auto port = reinterpret_cast<asio::serial_port*>(lua_touserdata(L, 1));
    if (!port || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &serial_port_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    port->open(static_cast<std::string>(tostringview(L, 2)), ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int serial_port_close(lua_State* L)
{
    auto port = reinterpret_cast<asio::serial_port*>(lua_touserdata(L, 1));
    if (!port || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &serial_port_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    port->close(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int serial_port_cancel(lua_State* L)
{
    auto port = reinterpret_cast<asio::serial_port*>(lua_touserdata(L, 1));
    if (!port || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &serial_port_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    port->cancel(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int serial_port_send_break(lua_State* L)
{
    auto port = reinterpret_cast<asio::serial_port*>(lua_touserdata(L, 1));
    if (!port || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &serial_port_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    port->send_break(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int serial_port_read_some(lua_State* L)
{
    lua_settop(L, 2);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto port = reinterpret_cast<asio::serial_port*>(lua_touserdata(L, 1));
    if (!port || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &serial_port_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto bs = reinterpret_cast<byte_span_handle*>(lua_touserdata(L, 2));
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
            auto port = reinterpret_cast<asio::serial_port*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            boost::system::error_code ignored_ec;
            port->cancel(ignored_ec);
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    port->async_read_some(
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

static int serial_port_write_some(lua_State* L)
{
    lua_settop(L, 2);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto port = reinterpret_cast<asio::serial_port*>(lua_touserdata(L, 1));
    if (!port || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &serial_port_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto bs = reinterpret_cast<byte_span_handle*>(lua_touserdata(L, 2));
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
            auto port = reinterpret_cast<asio::serial_port*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            boost::system::error_code ignored_ec;
            port->cancel(ignored_ec);
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    port->async_write_some(
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

inline int serial_port_is_open(lua_State* L)
{
    auto port = reinterpret_cast<asio::serial_port*>(lua_touserdata(L, 1));
    lua_pushboolean(L, port->is_open());
    return 1;
}

static int serial_port_mt_newindex(lua_State* L)
{
    auto port = reinterpret_cast<asio::serial_port*>(lua_touserdata(L, 1));
    boost::system::error_code ec;
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("baud_rate"),
                [&]() -> int {
                    luaL_checktype(L, 3, LUA_TNUMBER);
                    asio::serial_port_base::baud_rate o(lua_tointeger(L, 3));
                    port->set_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("flow_control"),
                [&]() -> int {
                    luaL_checktype(L, 3, LUA_TSTRING);
                    constexpr auto none =
                        asio::serial_port_base::flow_control::none;
                    constexpr auto software =
                        asio::serial_port_base::flow_control::software;
                    constexpr auto hardware =
                        asio::serial_port_base::flow_control::hardware;

                    return dispatch_table::dispatch(
                        hana::make_tuple(
                            hana::make_pair(
                                BOOST_HANA_STRING("none"),
                                [&]() -> int {
                                    asio::serial_port_base::flow_control o{
                                        none};
                                    port->set_option(o, ec);
                                    if (ec) {
                                        push(L, static_cast<std::error_code>(
                                            ec));
                                        return lua_error(L);
                                    }
                                    return 0;
                                }
                            ),
                            hana::make_pair(
                                BOOST_HANA_STRING("software"),
                                [&]() -> int {
                                    asio::serial_port_base::flow_control o{
                                        software};
                                    port->set_option(o, ec);
                                    if (ec) {
                                        push(L, static_cast<std::error_code>(
                                            ec));
                                        return lua_error(L);
                                    }
                                    return 0;
                                }
                            ),
                            hana::make_pair(
                                BOOST_HANA_STRING("hardware"),
                                [&]() -> int {
                                    asio::serial_port_base::flow_control o{
                                        hardware};
                                    port->set_option(o, ec);
                                    if (ec) {
                                        push(L, static_cast<std::error_code>(
                                            ec));
                                        return lua_error(L);
                                    }
                                    return 0;
                                }
                            )
                        ),
                        [L](std::string_view /*key*/) -> int {
                            push(L, std::errc::not_supported);
                            return lua_error(L);
                        },
                        tostringview(L, 3)
                    );
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("parity"),
                [&]() -> int {
                    luaL_checktype(L, 3, LUA_TSTRING);
                    constexpr auto none = asio::serial_port_base::parity::none;
                    constexpr auto odd = asio::serial_port_base::parity::odd;
                    constexpr auto even = asio::serial_port_base::parity::even;

                    return dispatch_table::dispatch(
                        hana::make_tuple(
                            hana::make_pair(
                                BOOST_HANA_STRING("none"),
                                [&]() -> int {
                                    asio::serial_port_base::parity o{none};
                                    port->set_option(o, ec);
                                    if (ec) {
                                        push(L, static_cast<std::error_code>(
                                            ec));
                                        return lua_error(L);
                                    }
                                    return 0;
                                }
                            ),
                            hana::make_pair(
                                BOOST_HANA_STRING("odd"),
                                [&]() -> int {
                                    asio::serial_port_base::parity o{odd};
                                    port->set_option(o, ec);
                                    if (ec) {
                                        push(L, static_cast<std::error_code>(
                                            ec));
                                        return lua_error(L);
                                    }
                                    return 0;
                                }
                            ),
                            hana::make_pair(
                                BOOST_HANA_STRING("even"),
                                [&]() -> int {
                                    asio::serial_port_base::parity o{even};
                                    port->set_option(o, ec);
                                    if (ec) {
                                        push(L, static_cast<std::error_code>(
                                            ec));
                                        return lua_error(L);
                                    }
                                    return 0;
                                }
                            )
                        ),
                        [L](std::string_view /*key*/) -> int {
                            push(L, std::errc::not_supported);
                            return lua_error(L);
                        },
                        tostringview(L, 3)
                    );
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("stop_bits"),
                [&]() -> int {
                    luaL_checktype(L, 3, LUA_TSTRING);
                    constexpr auto one = asio::serial_port_base::stop_bits::one;
                    constexpr auto onepointfive =
                        asio::serial_port_base::stop_bits::onepointfive;
                    constexpr auto two = asio::serial_port_base::stop_bits::two;

                    return dispatch_table::dispatch(
                        hana::make_tuple(
                            hana::make_pair(
                                BOOST_HANA_STRING("one"),
                                [&]() -> int {
                                    asio::serial_port_base::stop_bits o{one};
                                    port->set_option(o, ec);
                                    if (ec) {
                                        push(L, static_cast<std::error_code>(
                                            ec));
                                        return lua_error(L);
                                    }
                                    return 0;
                                }
                            ),
                            hana::make_pair(
                                BOOST_HANA_STRING("one_point_five"),
                                [&]() -> int {
                                    asio::serial_port_base::stop_bits o{
                                        onepointfive};
                                    port->set_option(o, ec);
                                    if (ec) {
                                        push(L, static_cast<std::error_code>(
                                            ec));
                                        return lua_error(L);
                                    }
                                    return 0;
                                }
                            ),
                            hana::make_pair(
                                BOOST_HANA_STRING("two"),
                                [&]() -> int {
                                    asio::serial_port_base::stop_bits o{two};
                                    port->set_option(o, ec);
                                    if (ec) {
                                        push(L, static_cast<std::error_code>(
                                            ec));
                                        return lua_error(L);
                                    }
                                    return 0;
                                }
                            )
                        ),
                        [L](std::string_view /*key*/) -> int {
                            push(L, std::errc::not_supported);
                            return lua_error(L);
                        },
                        tostringview(L, 3)
                    );
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("character_size"),
                [&]() -> int {
                    luaL_checktype(L, 3, LUA_TNUMBER);
                    asio::serial_port_base::character_size o(
                        lua_tointeger(L, 3));
                    port->set_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
            )
        ),
        [L](std::string_view /*key*/) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        },
        tostringview(L, 2)
    );
}

static int serial_port_mt_index(lua_State* L)
{
    auto port = reinterpret_cast<asio::serial_port*>(lua_touserdata(L, 1));
    boost::system::error_code ec;
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("open"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, serial_port_open);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("close"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, serial_port_close);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("cancel"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, serial_port_cancel);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("send_break"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, serial_port_send_break);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("read_some"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX, &serial_port_read_some_key);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("write_some"),
                [](lua_State* L) -> int {
                    rawgetp(L, LUA_REGISTRYINDEX, &serial_port_write_some_key);
                    return 1;
                }
            ),
            hana::make_pair(BOOST_HANA_STRING("is_open"), serial_port_is_open),
            hana::make_pair(
                BOOST_HANA_STRING("baud_rate"),
                [&](lua_State* L) -> int {
                    asio::serial_port_base::baud_rate o;
                    port->get_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    lua_pushinteger(L, o.value());
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("flow_control"),
                [&](lua_State* L) -> int {
                    asio::serial_port_base::flow_control o;
                    port->get_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    switch (o.value()) {
                    case asio::serial_port_base::flow_control::none:
                        lua_pushliteral(L, "none");
                        break;
                    case asio::serial_port_base::flow_control::software:
                        lua_pushliteral(L, "software");
                        break;
                    case asio::serial_port_base::flow_control::hardware:
                        lua_pushliteral(L, "hardware");
                    }
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("parity"),
                [&](lua_State* L) -> int {
                    asio::serial_port_base::parity o;
                    port->get_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    switch (o.value()) {
                    case asio::serial_port_base::parity::none:
                        lua_pushliteral(L, "none");
                        break;
                    case asio::serial_port_base::parity::odd:
                        lua_pushliteral(L, "odd");
                        break;
                    case asio::serial_port_base::parity::even:
                        lua_pushliteral(L, "even");
                    }
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("stop_bits"),
                [&](lua_State* L) -> int {
                    asio::serial_port_base::stop_bits o;
                    port->get_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    switch (o.value()) {
                    case asio::serial_port_base::stop_bits::one:
                        lua_pushliteral(L, "one");
                        break;
                    case asio::serial_port_base::stop_bits::onepointfive:
                        lua_pushliteral(L, "one_point_five");
                        break;
                    case asio::serial_port_base::stop_bits::two:
                        lua_pushliteral(L, "two");
                    }
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("character_size"),
                [&](lua_State* L) -> int {
                    asio::serial_port_base::character_size o;
                    port->get_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    lua_pushinteger(L, o.value());
                    return 1;
                }
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

static int serial_port_new(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);
    auto sock = reinterpret_cast<asio::serial_port*>(
        lua_newuserdata(L, sizeof(asio::serial_port))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &serial_port_mt_key);
    setmetatable(L, -2);
    new (sock) asio::serial_port{vm_ctx.strand().context()};
    return 1;
}

void init_serial_port(lua_State* L)
{
    lua_pushlightuserdata(L, &serial_port_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/1);

        lua_pushliteral(L, "new");
        lua_pushcfunction(L, serial_port_new);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &serial_port_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/4);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "serial_port");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__newindex");
        lua_pushcfunction(L, serial_port_mt_newindex);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, serial_port_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<asio::serial_port>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &serial_port_read_some_key);
    rawgetp(L, LUA_REGISTRYINDEX,
            &var_args__retval1_to_error__fwd_retval2__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, serial_port_read_some);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &serial_port_write_some_key);
    rawgetp(L, LUA_REGISTRYINDEX,
            &var_args__retval1_to_error__fwd_retval2__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, serial_port_write_some);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);
}

} // namespace emilua
