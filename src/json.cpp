/* Copyright (c) 2020 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

EMILUA_GPERF_DECLS_BEGIN(includes)
#include <emilua/json.hpp>

#include <emilua/detail/core.hpp>

#include <boost/hana/functional/overload.hpp>

#include <trial/protocol/buffer/string.hpp>
#include <trial/protocol/json/reader.hpp>
#include <trial/protocol/json/writer.hpp>
EMILUA_GPERF_DECLS_END(includes)

namespace emilua {

EMILUA_GPERF_DECLS_BEGIN(writer)
EMILUA_GPERF_NAMESPACE(emilua)
namespace json = trial::protocol::json;
namespace token = trial::protocol::json::token;
EMILUA_GPERF_DECLS_END(writer)

class json_category_impl: public std::error_category
{
public:
    const char* name() const noexcept override;
    std::string message(int value) const noexcept override;
};

EMILUA_GPERF_DECLS_BEGIN(writer)
EMILUA_GPERF_NAMESPACE(emilua)
struct json_writer
{
    json_writer()
        : writer(buffer)
    {}

    std::string buffer;
    json::writer writer;
};

static char json_array_mt_key;
static char json_null_key;
static char writer_mt_key;
EMILUA_GPERF_DECLS_END(writer)

extern unsigned char json_encode_bytecode[];
extern std::size_t json_encode_bytecode_size;

char json_key;

const char* json_category_impl::name() const noexcept
{
    return "emilua.json.dom";
}

std::string json_category_impl::message(int value) const noexcept
{
    switch (value) {
    case static_cast<int>(json_errc::result_out_of_range):
        return "Numerical result out of range";
    case static_cast<int>(json_errc::too_many_levels):
        return "Too many levels";
    case static_cast<int>(json_errc::array_too_long):
        return "Array too long";
    case static_cast<int>(json_errc::cycle_exists):
        return "Reference cycle exists";
    default:
        return {};
    }
}

const std::error_category& json_category()
{
    static json_category_impl cat;
    return cat;
}

static int is_array(lua_State* L)
{
    if (!lua_getmetatable(L, 1)) {
        lua_pushboolean(L, 0);
        return 1;
    }
    rawgetp(L, LUA_REGISTRYINDEX, &json_array_mt_key);
    lua_pushboolean(L, lua_rawequal(L, -1, -2));
    return 1;
}

static int into_array(lua_State* L)
{
    if (lua_gettop(L) == 0) {
        lua_newtable(L);
        rawgetp(L, LUA_REGISTRYINDEX, &json_array_mt_key);
        setmetatable(L, -2);
        return 1;
    }

    if (lua_type(L, 1) != LUA_TTABLE) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    if (lua_getmetatable(L, 1)) {
        rawgetp(L, LUA_REGISTRYINDEX, &json_array_mt_key);
        if (lua_rawequal(L, -1, -2)) {
            lua_pushvalue(L, 1);
            return 1;
        }

        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    rawgetp(L, LUA_REGISTRYINDEX, &json_array_mt_key);
    setmetatable(L, 1);
    lua_pushvalue(L, 1);
    return 1;
}

static int decode(lua_State* L)
{
    if (lua_type(L, 1) != LUA_TSTRING) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    // arg `n` from lua_raw{g,s}eti()
    using array_key_type = int;
    constexpr auto array_key_max = std::numeric_limits<array_key_type>::max();

    json::reader reader(tostringview(L, 1));
    std::string str_buf;
    std::vector<std::variant<std::string, array_key_type>> path;

    // During the JSON tree traversal, we always keep two values on top of the
    // lua stack (from top to bottom):
    //
    // * -1: Current work item (or `nil` if we don't have one yet).
    // * -2: The items tree stack.
    //
    // As we iterate using `reader`, the tree stack represents current level of
    // nested containers. It contains every (partially filled) DOM tree node
    // from current path. As nodes become complete on the closing token, it's
    // popped from the stack and inserted at the appropriate field on the new
    // stack top's node.
    //
    // Reserving a separate lua table to act as the parsing tree stack saves us
    // from constantly calling lua_checkstack(). Plus, the code is much easier
    // to follow.
    //
    // Keeping the current work item on the top of the lua stack saves us from
    // repeatedly retrieving it from the top of the tree stack during
    // array/object filling.
    lua_newtable(L);
    lua_pushnil(L);

    auto on_leaf = [&](const auto& val) -> json_errc {
        static constexpr auto push_leaf = hana::overload(
            [](lua_State* L, bool b) { lua_pushboolean(L, b ? 1 : 0); },
            [](lua_State* L, lua_Integer i) { lua_pushinteger(L, i); },
            [](lua_State* L, lua_Number n) { lua_pushnumber(L, n); },
            [](lua_State* L, std::string_view v) { push(L, v); },
            [](lua_State* L, std::monostate) {
                rawgetp(L, LUA_REGISTRYINDEX, &json_null_key);
            }
        );

        if (path.size() == 0) {
            lua_pop(L, 1);
            push_leaf(L, val);
            // next token is `end`, nothing to do
            return {};
        }

        return std::visit(hana::overload(
            [&](const std::string& key) -> json_errc {
                lua_pushlstring(L, key.data(), key.size());
                push_leaf(L, val);
                lua_rawset(L, -3);
                return {};
            },
            [&](array_key_type& key) -> json_errc {
                if (key == array_key_max)
                    return json_errc::array_too_long;

                push_leaf(L, val);
                lua_rawseti(L, -2, ++key);
                return {};
            }
        ), path.back());
    };

    auto append_path_info_to_exception = [&]() {
        lua_pushliteral(L, "path");
        {
            auto path_size = static_cast<array_key_type>(path.size());
            lua_createtable(L, /*narr=*/path_size, /*nrec=*/0);

            for (array_key_type i = 0 ; i != path_size ; ++i) {
                std::visit(hana::overload(
                    [L](const std::string& key) {
                        lua_pushlstring(L, key.data(), key.size());
                    },
                    [L](array_key_type key) {
                        // `key + 1` might overflow array_key_type
                        lua_Number idx = key;
                        lua_pushnumber(L, ++idx);
                    }
                ), path[i]);
                lua_rawseti(L, -2, i + 1);
            }
        }
        lua_rawset(L, -3);
    };

    do {
        json_errc ec{};
        switch (auto sym = reader.symbol()) {
        case json::token::symbol::error:
            push(L, reader.error());
            return lua_error(L);
        case json::token::symbol::end:
            push(L, json::errc::insufficient_tokens);
            return lua_error(L);
        case json::token::symbol::boolean:
            ec = on_leaf(reader.value<bool>());
            break;
        case json::token::symbol::integer: {
            lua_Integer val;
            try {
                val = reader.value<lua_Integer>();
            } catch (const json::error& e) {
                auto ec = e.code();
                if (e.code() == json::errc::invalid_value) {
                    // `invalid_value` doesn't convey much information
                    ec = json_errc::result_out_of_range;
                }
                push(L, ec);
                append_path_info_to_exception();
                return lua_error(L);
            }
            ec = on_leaf(val);
            break;
        }
        case json::token::symbol::real: {
            lua_Number val;
            try {
                val = reader.value<lua_Number>();
            } catch (const json::error& e) {
                auto ec = e.code();
                if (e.code() == json::errc::invalid_value) {
                    // `invalid_value` doesn't convey much information
                    ec = json_errc::result_out_of_range;
                }
                push(L, ec);
                append_path_info_to_exception();
                return lua_error(L);
            }
            ec = on_leaf(val);
            break;
        }
        case json::token::symbol::key:
        case json::token::symbol::string:
            str_buf.clear();
            reader.string(str_buf);
            if (sym == json::token::symbol::key)
                path.back().emplace<std::string>(str_buf);
            else
                ec = on_leaf(str_buf);
            break;
        case json::token::symbol::null:
            ec = on_leaf(std::monostate{});
            break;
        case json::token::symbol::begin_array:
        case json::token::symbol::begin_object:
            if (path.size() == array_key_max) {
                push(L, json_errc::too_many_levels);
                // We don't append path info here for the following reasons:
                //
                // * The error is not the current path fault. Other items in the
                //   same JSON tree might have even deeper structures.
                // * If the error reason is `too_many_levels`, then there is a
                //   bigger risk of exhausting memory by producing the error
                //   message if we append path info. If we shut the VM on ENOMEM
                //   on this event, then we're not really protecting the user
                //   against malformed JSON data. Thus, we'd contradict the
                //   protection from checking against deep levels.
                return lua_error(L);
            }

            lua_newtable(L);

            if (path.size() > 0) {
                lua_pushvalue(L, -1);

                ec = std::visit(hana::overload(
                    [&](const std::string& key) -> json_errc {
                        lua_pushlstring(L, key.data(), key.size());
                        lua_insert(L, -2);
                        lua_rawset(L, -4);
                        return {};
                    },
                    [&](array_key_type& key) -> json_errc {
                        if (key == array_key_max)
                            return json_errc::array_too_long;

                        lua_rawseti(L, -3, ++key);
                        return {};
                    }
                ), path.back());
                if (ec != json_errc{})
                    break;
            }

            if (sym == json::token::symbol::begin_array) {
                path.emplace_back(std::in_place_type<array_key_type>, 0);

                rawgetp(L, LUA_REGISTRYINDEX, &json_array_mt_key);
                setmetatable(L, -2);
            } else {
                path.emplace_back(std::in_place_type<std::string>);
            }

            lua_remove(L, -2);
            lua_pushvalue(L, -1);
            lua_rawseti(L, -3, static_cast<array_key_type>(path.size()));
            break;
        case json::token::symbol::end_array:
        case json::token::symbol::end_object:
            path.pop_back();
            if (path.size() == 0) {
                // next token is `end`, nothing to do
                break;
            }

            lua_pop(L, 1);
            lua_pushnil(L);
            lua_rawseti(L, -2, static_cast<array_key_type>(path.size() + 1));
            lua_rawgeti(L, -1, static_cast<array_key_type>(path.size()));
        }
        if (ec != json_errc{}) {
            push(L, ec);
            if (ec == json_errc::array_too_long)
                path.pop_back();
            append_path_info_to_exception();
            return lua_error(L);
        }
    } while (reader.next());

    if (reader.symbol() == json::token::symbol::error) {
        push(L, reader.error());
        return lua_error(L);
    }

    assert(lua_objlen(L, -2) <= 1);
    return 1;
}

static int get_tojson(lua_State* L)
{
    if (!lua_getmetatable(L, 1))
        return 0;

    lua_pushliteral(L, "__tojson");
    lua_rawget(L, -2); //< rawget to intentionally bypass metamethods, as we
                       //< don't want a C frame between lua stacks
    return 1;
}

EMILUA_GPERF_DECLS_BEGIN(writer)
EMILUA_GPERF_NAMESPACE(emilua)
static int writer_value(lua_State* L)
{
    lua_settop(L, 2);
    auto jw = static_cast<json_writer*>(lua_touserdata(L, 1));
    if (!jw || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &writer_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    try {
        switch (lua_type(L, 2)) {
        case LUA_TTABLE:
            rawgetp(L, LUA_REGISTRYINDEX, &json_null_key);
            if (!lua_rawequal(L, -1, 2)) {
        case LUA_TNIL:
        case LUA_TFUNCTION:
        case LUA_TUSERDATA:
        case LUA_TTHREAD:
        case LUA_TLIGHTUSERDATA:
                push(L, std::errc::invalid_argument, "arg", 2);
                return lua_error(L);
            }
            if (jw->writer.value<json::token::null>() == 0) {
                push(L, jw->writer.error());
                return lua_error(L);
            }
            break;
        case LUA_TNUMBER:
            if (jw->writer.value(lua_tonumber(L, 2)) == 0) {
                push(L, jw->writer.error());
                return lua_error(L);
            }
            break;
        case LUA_TBOOLEAN:
            if (jw->writer.value(lua_toboolean(L, 2) ? true : false) == 0) {
                push(L, jw->writer.error());
                return lua_error(L);
            }
            break;
        case LUA_TSTRING:
            if (jw->writer.value(tostringview(L, 2)) == 0) {
                push(L, jw->writer.error());
                return lua_error(L);
            }
        }
    } catch (const json::error& e) {
        push(L, e.code());
        return lua_error(L);
    }

    return 0;
}

template<class T>
static int writer_token(lua_State* L)
{
    auto jw = static_cast<json_writer*>(lua_touserdata(L, 1));
    if (!jw || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &writer_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    try {
        if (jw->writer.value<T>() == 0) {
            push(L, jw->writer.error());
            return lua_error(L);
        }
    } catch (const json::error& e) {
        push(L, e.code());
        return lua_error(L);
    }

    return 0;
}

static int writer_literal(lua_State* L)
{
    lua_settop(L, 2);
    auto jw = static_cast<json_writer*>(lua_touserdata(L, 1));
    if (!jw || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &writer_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    if (lua_type(L, 2) != LUA_TSTRING) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    try {
        if (jw->writer.literal(tostringview(L, 2)) == 0) {
            push(L, jw->writer.error());
            return lua_error(L);
        }
    } catch (const json::error& e) {
        push(L, e.code());
        return lua_error(L);
    }

    return 0;
}

static int writer_generate(lua_State* L)
{
    auto jw = static_cast<json_writer*>(lua_touserdata(L, 1));
    if (!jw || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &writer_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    lua_pushlstring(L, jw->buffer.data(), jw->buffer.size());
    lua_pushnil(L);
    setmetatable(L, 1);
    jw->~json_writer();
    return 1;
}

inline int writer_level(lua_State* L)
{
    auto jw = static_cast<json_writer*>(lua_touserdata(L, 1));
    assert(jw);
    lua_pushnumber(L, jw->writer.level());
    return 1;
}
EMILUA_GPERF_DECLS_END(writer)

static int writer_mt_index(lua_State* L)
{
    auto key = tostringview(L, 2);
    return EMILUA_GPERF_BEGIN(key)
        EMILUA_GPERF_PARAM(int (*action)(lua_State*))
        EMILUA_GPERF_DEFAULT_VALUE([](lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        })
        EMILUA_GPERF_PAIR(
            "value",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, writer_value);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "begin_object",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, writer_token<json::token::begin_object>);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "end_object",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, writer_token<json::token::end_object>);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "begin_array",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, writer_token<json::token::begin_array>);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "end_array",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, writer_token<json::token::end_array>);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "literal",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, writer_literal);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "generate",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, writer_generate);
                return 1;
            })
        EMILUA_GPERF_PAIR("level", writer_level)
    EMILUA_GPERF_END(key)(L);
}

static int writer_new(lua_State* L)
{
    auto jw = static_cast<json_writer*>(
        lua_newuserdata(L, sizeof(json_writer))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &writer_mt_key);
    setmetatable(L, -2);
    new (jw) json_writer{};
    return 1;
}

void init_json_module(lua_State* L)
{
    lua_pushlightuserdata(L, &json_array_mt_key);
    lua_newtable(L);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &json_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/6);

        lua_pushliteral(L, "lexer_ecat");
        {
            *static_cast<const std::error_category**>(
                lua_newuserdata(L, sizeof(void*))) = &json::error_category();
            rawgetp(L, LUA_REGISTRYINDEX, &detail::error_category_mt_key);
            setmetatable(L, -2);
        }
        lua_rawset(L, -3);

        lua_pushliteral(L, "dom_ecat");
        {
            *static_cast<const std::error_category**>(
                lua_newuserdata(L, sizeof(void*))) = &json_category();
            rawgetp(L, LUA_REGISTRYINDEX, &detail::error_category_mt_key);
            setmetatable(L, -2);
        }
        lua_rawset(L, -3);

        lua_pushliteral(L, "null");
        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/0);
            lua_pushlightuserdata(L, &json_null_key);
            lua_pushvalue(L, -2);
            lua_rawset(L, LUA_REGISTRYINDEX);

            {
                lua_createtable(L, /*narr=*/0, /*nrec=*/4);

                lua_pushliteral(L, "__metatable");
                lua_pushliteral(L, "json-null");
                lua_rawset(L, -3);

                lua_pushliteral(L, "__index");
                lua_pushcfunction(
                    L,
                    [](lua_State* L) -> int {
                        push(L, errc::bad_index);
                        return lua_error(L);
                    });
                lua_rawset(L, -3);

                lua_pushliteral(L, "__newindex");
                lua_pushcfunction(
                    L,
                    [](lua_State* L) -> int {
                        push(L, std::errc::operation_not_permitted);
                        return lua_error(L);
                    });
                lua_rawset(L, -3);

                lua_pushliteral(L, "__tostring");
                lua_pushcfunction(
                    L,
                    [](lua_State* L) -> int {
                        lua_pushliteral(L, "null");
                        return 1;
                    });
                lua_rawset(L, -3);
            }

            setmetatable(L, -2);
        }
        lua_rawset(L, -3);

        lua_pushliteral(L, "is_array");
        lua_pushcfunction(L, is_array);
        lua_rawset(L, -3);

        lua_pushliteral(L, "into_array");
        lua_pushcfunction(L, into_array);
        lua_rawset(L, -3);

        lua_pushliteral(L, "decode");
        lua_pushcfunction(L, decode);
        lua_rawset(L, -3);

        lua_pushliteral(L, "encode");
        {
            int res = luaL_loadbuffer(
                L, reinterpret_cast<char*>(json_encode_bytecode),
                json_encode_bytecode_size, nullptr);
            assert(res == 0); boost::ignore_unused(res);
            lua_pushcfunction(L, writer_new);
            lua_pushcfunction(L, get_tojson);
            lua_pushcfunction(L, is_array);
            rawgetp(L, LUA_REGISTRYINDEX, &json_null_key);
            rawgetp(L, LUA_REGISTRYINDEX, &raw_type_key);
            rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
            push(L, std::errc::invalid_argument);
            push(L, json_errc::cycle_exists);
            push(L, std::errc::not_supported);
            rawgetp(L, LUA_REGISTRYINDEX, &raw_pairs_key);
            rawgetp(L, LUA_REGISTRYINDEX, &raw_ipairs_key);
            lua_call(L, 11, 1);
        }
        lua_rawset(L, -3);

        lua_pushliteral(L, "writer");
        {
            lua_newtable(L);

            lua_pushliteral(L, "new");
            lua_pushcfunction(L, writer_new);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &writer_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "json.writer");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, writer_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<json_writer>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);
}

} // namespace emilua
