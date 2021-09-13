#include <emilua/byte_span.hpp>

#include <cstring>

#include <boost/safe_numerics/safe_integer.hpp>

#include <emilua/dispatch_table.hpp>

namespace emilua {

char byte_span_key;
char byte_span_mt_key;

static int byte_span_new(lua_State* L)
{
    if (lua_type(L, 1) != LUA_TNUMBER) {
        push(L, std::errc::invalid_argument);
        return lua_error(L);
    }
    lua_Integer length = lua_tointeger(L, 1);

    lua_Integer capacity;
    switch (lua_type(L, 2)) {
    case LUA_TNIL:
    case LUA_TNONE:
        capacity = length;
        break;
    case LUA_TNUMBER:
        capacity = lua_tointeger(L, 2);
        break;
    default:
        push(L, std::errc::invalid_argument);
        return lua_error(L);
    }

    if (length < 0 || capacity < 1 || length > capacity) {
        push(L, std::errc::invalid_argument);
        return lua_error(L);
    }

    auto bs = reinterpret_cast<byte_span_handle*>(
        lua_newuserdata(L, sizeof(byte_span_handle))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &byte_span_mt_key);
    setmetatable(L, -2);
    new (bs) byte_span_handle{length, capacity};
    return 1;
}

static int byte_span_mt_len(lua_State* L)
{
    auto bs = reinterpret_cast<byte_span_handle*>(lua_touserdata(L, 1));
    lua_pushinteger(L, bs->size);
    return 1;
}

static int byte_span_mt_eq(lua_State* L)
{
    auto bs1 = reinterpret_cast<byte_span_handle*>(lua_touserdata(L, 1));
    auto bs2 = reinterpret_cast<byte_span_handle*>(lua_touserdata(L, 2));
    lua_pushboolean(
        L,
        std::string_view(reinterpret_cast<char*>(bs1->data.get()), bs1->size) ==
        std::string_view(reinterpret_cast<char*>(bs2->data.get()), bs2->size));
    return 1;
}

static int byte_span_mt_tostring(lua_State* L)
{
    auto bs = reinterpret_cast<byte_span_handle*>(lua_touserdata(L, 1));
    std::string_view sv(reinterpret_cast<char*>(bs->data.get()), bs->size);
    push(L, sv);
    return 1;
}

static int byte_span_slice(lua_State* L)
{
    lua_settop(L, 3);

    auto bs = reinterpret_cast<byte_span_handle*>(lua_touserdata(L, 1));
    if (!bs || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &byte_span_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    lua_Integer start, end;

    switch (lua_type(L, 2)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    case LUA_TNUMBER:
        start = lua_tointeger(L, 2);
        break;
    case LUA_TNIL:
    case LUA_TNONE:
        start = 1;
    }

    switch (lua_type(L, 3)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    case LUA_TNUMBER:
        end = lua_tointeger(L, 3);
        break;
    case LUA_TNIL:
    case LUA_TNONE:
        end = bs->size;
    }

    if (start < 1 || start - 1 > end || end > bs->capacity ||
        (bs->capacity - start + 1 == 0)) {
        push(L, std::errc::result_out_of_range);
        return lua_error(L);
    }

    std::shared_ptr<unsigned char[]> new_data(
        bs->data,
        bs->data.get() + start - 1
    );

    auto new_bs = reinterpret_cast<byte_span_handle*>(
        lua_newuserdata(L, sizeof(byte_span_handle))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &byte_span_mt_key);
    setmetatable(L, -2);
    new (new_bs) byte_span_handle{
        std::move(new_data),
        end - start + 1,
        bs->capacity - start + 1
    };
    return 1;
}

static int byte_span_copy(lua_State* L)
{
    lua_settop(L, 2);

    auto bs = reinterpret_cast<byte_span_handle*>(lua_touserdata(L, 1));
    if (!bs || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &byte_span_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    std::string_view src;
    switch (lua_type(L, 2)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    case LUA_TSTRING:
        src = tostringview(L, 2);
        break;
    case LUA_TUSERDATA: {
        if (!lua_getmetatable(L, 2) || !lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        auto src_bs = reinterpret_cast<byte_span_handle*>(lua_touserdata(L, 2));
        src = std::string_view(
            reinterpret_cast<char*>(src_bs->data.get()), src_bs->size);
    }
    }

    auto count = std::min<std::size_t>(src.size(), bs->size);
    std::memmove(bs->data.get(), src.data(), count);

    lua_pushinteger(L, count);
    return 1;
}

static int byte_span_append(lua_State* L)
{
    int nargs = lua_gettop(L);

    auto bs = reinterpret_cast<byte_span_handle*>(lua_touserdata(L, 1));
    if (!bs || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &byte_span_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    if (nargs == 1) {
        lua_pushvalue(L, 1);
        return 1;
    }

    std::vector<std::string_view> tail_slices;
    tail_slices.reserve(nargs - 1);

    for (int i = 2 ; i <= nargs ; ++i) {
        switch (lua_type(L, i)) {
        default:
            push(L, std::errc::invalid_argument, "arg", i);
            return lua_error(L);
        case LUA_TSTRING:
            tail_slices.emplace_back(tostringview(L, i));
            break;
        case LUA_TUSERDATA: {
            if (!lua_getmetatable(L, i) || !lua_rawequal(L, -1, -2)) {
                push(L, std::errc::invalid_argument, "arg", i);
                return lua_error(L);
            }
            lua_pop(L, 1);
            auto src_bs = reinterpret_cast<byte_span_handle*>(
                lua_touserdata(L, i));
            tail_slices.emplace_back(
                reinterpret_cast<char*>(src_bs->data.get()), src_bs->size);
        }
        }
    }

    try {
        boost::safe_numerics::safe<lua_Integer> total_size = bs->size;
        for (const auto& s: tail_slices) total_size += s.size();

        auto dst_bs = reinterpret_cast<byte_span_handle*>(
            lua_newuserdata(L, sizeof(byte_span_handle))
        );
        rawgetp(L, LUA_REGISTRYINDEX, &byte_span_mt_key);
        setmetatable(L, -2);

        if (bs->capacity >= total_size) {
            new (dst_bs) byte_span_handle{bs->data, total_size, bs->capacity};
        } else {
            new (dst_bs) byte_span_handle{total_size, total_size};
            std::memcpy(dst_bs->data.get(), bs->data.get(), bs->size);
        }

        std::size_t idx = bs->size;
        for (const auto& s: tail_slices) {
            std::memcpy(dst_bs->data.get() + idx, s.data(), s.size());
            idx += s.size();
        }

        return 1;
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    }
}

inline int byte_span_capacity(lua_State* L)
{
    auto bs = reinterpret_cast<byte_span_handle*>(lua_touserdata(L, 1));
    lua_pushinteger(L, bs->capacity);
    return 1;
}

static int byte_span_mt_index(lua_State* L)
{
    if (lua_type(L, 2) == LUA_TNUMBER) {
        auto bs = reinterpret_cast<byte_span_handle*>(lua_touserdata(L, 1));
        lua_Integer idx = lua_tointeger(L, 2);
        if (idx < 1 || idx > bs->size) {
            push(L, std::errc::result_out_of_range);
            return lua_error(L);
        }

        lua_pushinteger(L, bs->data[idx - 1]);
        return 1;
    }

    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(
                BOOST_HANA_STRING("slice"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, byte_span_slice);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("copy"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, byte_span_copy);
                    return 1;
                }
            ),
            hana::make_pair(
                BOOST_HANA_STRING("append"),
                [](lua_State* L) -> int {
                    lua_pushcfunction(L, byte_span_append);
                    return 1;
                }
            ),
            hana::make_pair(BOOST_HANA_STRING("capacity"), byte_span_capacity)
        ),
        [](std::string_view /*key*/, lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        },
        tostringview(L, 2),
        L
    );
}

static int byte_span_mt_newindex(lua_State* L)
{
    if (lua_type(L, 2) != LUA_TNUMBER) {
        push(L, std::errc::operation_not_permitted);
        return lua_error(L);
    }

    if (lua_type(L, 3) != LUA_TNUMBER) {
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    }

    auto bs = reinterpret_cast<byte_span_handle*>(lua_touserdata(L, 1));
    lua_Integer idx = lua_tointeger(L, 2);
    lua_Integer newvalue = lua_tointeger(L, 3);

    if (idx < 1 || idx > bs->size || newvalue < 0 || newvalue > 0xFF) {
        push(L, std::errc::result_out_of_range);
        return lua_error(L);
    }

    bs->data[idx - 1] = static_cast<unsigned char>(newvalue);
    return 0;
}

void init_byte_span(lua_State* L)
{
    lua_pushlightuserdata(L, &byte_span_key);
    lua_createtable(L, /*narr=*/0, /*nrec=*/1);
    {
        lua_pushliteral(L, "new");
        lua_pushcfunction(L, byte_span_new);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &byte_span_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/7);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "byte_span");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, byte_span_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__newindex");
        lua_pushcfunction(L, byte_span_mt_newindex);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__len");
        lua_pushcfunction(L, byte_span_mt_len);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__eq");
        lua_pushcfunction(L, byte_span_mt_eq);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__tostring");
        lua_pushcfunction(L, byte_span_mt_tostring);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<byte_span_handle>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);
}

} // namespace emilua