#include <boost/asio/ip/address.hpp>

#include <emilua/dispatch_table.hpp>
#include <emilua/ip.hpp>

namespace emilua {

char ip_key;
char ip_address_mt_key;

static int address_new(lua_State* L)
{
    lua_settop(L, 1);
    auto a = reinterpret_cast<asio::ip::address*>(
        lua_newuserdata(L, sizeof(asio::ip::address))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    int res = lua_setmetatable(L, -2);
    assert(res); boost::ignore_unused(res);
    switch (lua_type(L, 1)) {
    case LUA_TNIL:
        new (a) asio::ip::address{};
        break;
    case LUA_TSTRING: {
        boost::system::error_code ec;
        new (a) asio::ip::address{
            asio::ip::make_address(lua_tostring(L, 1), ec)
        };
        if (ec) {
            push(L, static_cast<std::error_code>(ec));
            return lua_error(L);
        }
        break;
    }
    default:
        static_assert(std::is_trivially_destructible_v<asio::ip::address>);
        push(L, std::errc::invalid_argument);
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }
    return 1;
}

static int address_any_v4(lua_State* L)
{
    auto a = reinterpret_cast<asio::ip::address*>(
        lua_newuserdata(L, sizeof(asio::ip::address))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    int res = lua_setmetatable(L, -2);
    assert(res); boost::ignore_unused(res);
    new (a) asio::ip::address{asio::ip::address_v4::any()};
    return 1;
}

static int address_any_v6(lua_State* L)
{
    auto a = reinterpret_cast<asio::ip::address*>(
        lua_newuserdata(L, sizeof(asio::ip::address))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    int res = lua_setmetatable(L, -2);
    assert(res); boost::ignore_unused(res);
    new (a) asio::ip::address{asio::ip::address_v6::any()};
    return 1;
}

static int address_loopback_v4(lua_State* L)
{
    auto a = reinterpret_cast<asio::ip::address*>(
        lua_newuserdata(L, sizeof(asio::ip::address))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    int res = lua_setmetatable(L, -2);
    assert(res); boost::ignore_unused(res);
    new (a) asio::ip::address{asio::ip::address_v4::loopback()};
    return 1;
}

static int address_loopback_v6(lua_State* L)
{
    auto a = reinterpret_cast<asio::ip::address*>(
        lua_newuserdata(L, sizeof(asio::ip::address))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    int res = lua_setmetatable(L, -2);
    assert(res); boost::ignore_unused(res);
    new (a) asio::ip::address{asio::ip::address_v6::loopback()};
    return 1;
}

static int address_broadcast_v4(lua_State* L)
{
    auto a = reinterpret_cast<asio::ip::address*>(
        lua_newuserdata(L, sizeof(asio::ip::address))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    int res = lua_setmetatable(L, -2);
    assert(res); boost::ignore_unused(res);
    new (a) asio::ip::address{asio::ip::address_v4::broadcast()};
    return 1;
}

static int address_to_v6(lua_State* L)
{
    auto a = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 1));
    if (!a || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument);
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    if (!lua_rawequal(L, -1, -2) || !a->is_v4()) {
        push(L, std::errc::invalid_argument);
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }

    auto ret = reinterpret_cast<asio::ip::address*>(
        lua_newuserdata(L, sizeof(asio::ip::address))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    int res = lua_setmetatable(L, -2);
    assert(res); boost::ignore_unused(res);
    new (ret) asio::ip::address{
        asio::ip::make_address_v6(asio::ip::v4_mapped, a->to_v4())
    };

    return 1;
}

static int address_to_v4(lua_State* L)
{
    auto a = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 1));
    if (!a || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument);
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    if (!lua_rawequal(L, -1, -2) || !a->is_v6()) {
        push(L, std::errc::invalid_argument);
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }

    auto ret = reinterpret_cast<asio::ip::address*>(
        lua_newuserdata(L, sizeof(asio::ip::address))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    int res = lua_setmetatable(L, -2);
    assert(res); boost::ignore_unused(res);
    try {
        new (ret) asio::ip::address{
            asio::ip::make_address_v4(asio::ip::v4_mapped, a->to_v6())
        };
    } catch (const asio::ip::bad_address_cast&) {
        static_assert(std::is_trivially_destructible_v<asio::ip::address>);
        push(L, std::errc::invalid_argument);
        return lua_error(L);
    }

    return 1;
}

inline int address_is_loopback(lua_State* L)
{
    auto a = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 1));
    lua_pushboolean(L, a->is_loopback());
    return 1;
}

inline int address_is_multicast(lua_State* L)
{
    auto a = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 1));
    lua_pushboolean(L, a->is_multicast());
    return 1;
}

inline int address_is_unspecified(lua_State* L)
{
    auto a = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 1));
    lua_pushboolean(L, a->is_unspecified());
    return 1;
}

inline int address_is_v4(lua_State* L)
{
    auto a = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 1));
    lua_pushboolean(L, a->is_v4());
    return 1;
}

inline int address_is_v6(lua_State* L)
{
    auto a = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 1));
    lua_pushboolean(L, a->is_v6());
    return 1;
}

inline int address_is_link_local(lua_State* L)
{
    auto a = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 1));
    if (!a->is_v6()) {
        push(L, std::errc::invalid_argument);
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }

    lua_pushboolean(L, a->to_v6().is_link_local());
    return 1;
}

inline int address_is_multicast_global(lua_State* L)
{
    auto a = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 1));
    if (!a->is_v6()) {
        push(L, std::errc::invalid_argument);
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }

    lua_pushboolean(L, a->to_v6().is_multicast_global());
    return 1;
}

inline int address_is_multicast_link_local(lua_State* L)
{
    auto a = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 1));
    if (!a->is_v6()) {
        push(L, std::errc::invalid_argument);
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }

    lua_pushboolean(L, a->to_v6().is_multicast_link_local());
    return 1;
}

inline int address_is_multicast_node_local(lua_State* L)
{
    auto a = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 1));
    if (!a->is_v6()) {
        push(L, std::errc::invalid_argument);
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }

    lua_pushboolean(L, a->to_v6().is_multicast_node_local());
    return 1;
}

inline int address_is_multicast_org_local(lua_State* L)
{
    auto a = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 1));
    if (!a->is_v6()) {
        push(L, std::errc::invalid_argument);
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }

    lua_pushboolean(L, a->to_v6().is_multicast_org_local());
    return 1;
}

inline int address_is_multicast_site_local(lua_State* L)
{
    auto a = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 1));
    if (!a->is_v6()) {
        push(L, std::errc::invalid_argument);
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }

    lua_pushboolean(L, a->to_v6().is_multicast_site_local());
    return 1;
}

inline int address_is_site_local(lua_State* L)
{
    auto a = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 1));
    if (!a->is_v6()) {
        push(L, std::errc::invalid_argument);
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }

    lua_pushboolean(L, a->to_v6().is_site_local());
    return 1;
}

inline int address_is_v4_mapped(lua_State* L)
{
    auto a = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 1));
    if (!a->is_v6()) {
        push(L, std::errc::invalid_argument);
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }

    lua_pushboolean(L, a->to_v6().is_v4_mapped());
    return 1;
}

inline int address_scope_id_get(lua_State* L)
{
    auto a = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 1));
    if (!a->is_v6()) {
        push(L, std::errc::invalid_argument);
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }

    lua_pushnumber(L, a->to_v6().scope_id());
    return 1;
}

static int address_meta_index(lua_State* L)
{
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("to_v6"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, address_to_v6);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("to_v4"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, address_to_v4);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("is_loopback"), address_is_loopback),
            hana::make_pair(
                BOOST_HANA_STRING("is_multicast"), address_is_multicast),
            hana::make_pair(
                BOOST_HANA_STRING("is_unspecified"), address_is_unspecified),
            hana::make_pair(BOOST_HANA_STRING("is_v4"), address_is_v4),
            hana::make_pair(BOOST_HANA_STRING("is_v6"), address_is_v6),

            // v6-only properties
            hana::make_pair(
                BOOST_HANA_STRING("is_link_local"), address_is_link_local),
            hana::make_pair(
                BOOST_HANA_STRING("is_multicast_global"),
                address_is_multicast_global),
            hana::make_pair(
                BOOST_HANA_STRING("is_multicast_link_local"),
                address_is_multicast_link_local),
            hana::make_pair(
                BOOST_HANA_STRING("is_multicast_node_local"),
                address_is_multicast_node_local),
            hana::make_pair(
                BOOST_HANA_STRING("is_multicast_org_local"),
                address_is_multicast_org_local),
            hana::make_pair(
                BOOST_HANA_STRING("is_multicast_site_local"),
                address_is_multicast_site_local),
            hana::make_pair(
                BOOST_HANA_STRING("is_site_local"), address_is_site_local),
            hana::make_pair(
                BOOST_HANA_STRING("is_v4_mapped"), address_is_v4_mapped),
            hana::make_pair(BOOST_HANA_STRING("scope_id"), address_scope_id_get)
        ),
        [](std::string_view /*key*/, lua_State* L) -> int {
            push(L, errc::bad_index);
            lua_pushliteral(L, "index");
            lua_pushvalue(L, 2);
            lua_rawset(L, -3);
            return lua_error(L);
        },
        tostringview(L, 2),
        L
    );
}

inline int address_scope_id_set(lua_State* L)
{
    luaL_checktype(L, 3, LUA_TNUMBER);

    auto a = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 1));
    if (!a->is_v6()) {
        push(L, std::errc::invalid_argument);
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }

    auto as_v6 = a->to_v6();
    as_v6.scope_id(lua_tonumber(L, 3));
    *a = as_v6;
    return 0;
}

static int address_meta_newindex(lua_State* L)
{
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(BOOST_HANA_STRING("scope_id"), address_scope_id_set)
        ),
        [](std::string_view /*key*/, lua_State* L) -> int {
            push(L, errc::bad_index);
            lua_pushliteral(L, "index");
            lua_pushvalue(L, 2);
            lua_rawset(L, -3);
            return lua_error(L);
        },
        tostringview(L, 2),
        L
    );
}

static int address_meta_tostring(lua_State* L)
{
    auto a = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 1));
    auto ret = a->to_string();
    lua_pushlstring(L, ret.data(), ret.size());
    return 1;
}

static int address_meta_eq(lua_State* L)
{
    auto a1 = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 1));
    auto a2 = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 2));
    lua_pushboolean(L, *a1 == *a2);
    return 1;
}

static int address_meta_lt(lua_State* L)
{
    auto a1 = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 1));
    auto a2 = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 2));
    lua_pushboolean(L, *a1 < *a2);
    return 1;
}

static int address_meta_le(lua_State* L)
{
    auto a1 = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 1));
    auto a2 = reinterpret_cast<asio::ip::address*>(lua_touserdata(L, 2));
    lua_pushboolean(L, *a1 <= *a2);
    return 1;
}

void init_ip(lua_State* L)
{
    lua_pushlightuserdata(L, &ip_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/1);

        lua_pushliteral(L, "address");
        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/6);

            lua_pushliteral(L, "new");
            lua_pushcfunction(L, address_new);
            lua_rawset(L, -3);

            lua_pushliteral(L, "any_v4");
            lua_pushcfunction(L, address_any_v4);
            lua_rawset(L, -3);

            lua_pushliteral(L, "any_v6");
            lua_pushcfunction(L, address_any_v6);
            lua_rawset(L, -3);

            lua_pushliteral(L, "loopback_v4");
            lua_pushcfunction(L, address_loopback_v4);
            lua_rawset(L, -3);

            lua_pushliteral(L, "loopback_v6");
            lua_pushcfunction(L, address_loopback_v6);
            lua_rawset(L, -3);

            lua_pushliteral(L, "broadcast_v4");
            lua_pushcfunction(L, address_broadcast_v4);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &ip_address_mt_key);
    {
        static_assert(std::is_trivially_destructible_v<asio::ip::address>);

        lua_createtable(L, /*narr=*/0, /*nrec=*/7);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "ip.address");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, address_meta_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__newindex");
        lua_pushcfunction(L, address_meta_newindex);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__tostring");
        lua_pushcfunction(L, address_meta_tostring);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__eq");
        lua_pushcfunction(L, address_meta_eq);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__lt");
        lua_pushcfunction(L, address_meta_lt);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__le");
        lua_pushcfunction(L, address_meta_le);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);
}

} // namespace emilua
