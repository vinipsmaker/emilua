/* Copyright (c) 2022 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#include <emilua/regex.hpp>

#include <locale>
#include <regex>

#include <emilua/dispatch_table.hpp>
#include <emilua/byte_span.hpp>

namespace emilua {

char regex_key;
char regex_mt_key;

int regex_new(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);

    lua_getfield(L, 1, "pattern");
    luaL_checktype(L, -1, LUA_TSTRING);
    auto pattern = tostringview(L);

    std::regex::flag_type flags{};

    lua_getfield(L, 1, "grammar");
    luaL_checktype(L, -1, LUA_TSTRING);
    auto grammar = tostringview(L);
    if (grammar == "ecma") {
        flags |= std::regex_constants::ECMAScript;
    } else if (grammar == "basic") {
        flags |= std::regex_constants::basic;
    } else if (grammar == "extended") {
        flags |= std::regex_constants::extended;
    } else {
        push(L, std::errc::invalid_argument, "arg", "grammar");
        return lua_error(L);
    }

    lua_getfield(L, 1, "ignore_case");
    switch (lua_type(L, -1)) {
    case LUA_TNIL:
        break;
    case LUA_TBOOLEAN:
        if (lua_toboolean(L, -1) == 1)
            flags |= std::regex_constants::icase;
        break;
    default:
        push(L, std::errc::invalid_argument, "arg", "ignore_case");
        return lua_error(L);
    }

    lua_getfield(L, 1, "nosubs");
    switch (lua_type(L, -1)) {
    case LUA_TNIL:
        break;
    case LUA_TBOOLEAN:
        if (lua_toboolean(L, -1) == 1)
            flags |= std::regex_constants::nosubs;
        break;
    default:
        push(L, std::errc::invalid_argument, "arg", "nosubs");
        return lua_error(L);
    }

    lua_getfield(L, 1, "optimize");
    switch (lua_type(L, -1)) {
    case LUA_TNIL:
        break;
    case LUA_TBOOLEAN:
        if (lua_toboolean(L, -1) == 1)
            flags |= std::regex_constants::optimize;
        break;
    default:
        push(L, std::errc::invalid_argument, "arg", "optimize");
        return lua_error(L);
    }

    auto regex = static_cast<std::regex*>(
        lua_newuserdata(L, sizeof(std::regex))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &regex_mt_key);
    setmetatable(L, -2);
    try {
        new (regex) std::regex{};
        regex->imbue(std::locale::classic());
        regex->assign(pattern.data(), pattern.size(), flags);
    } catch (const std::regex_error& e) {
        lua_pushnil(L);
        setmetatable(L, -2);
        push(L, std::error_code(e.code(), regex_category()));
        return lua_error(L);
    } catch (const std::system_error& e) {
        lua_pushnil(L);
        setmetatable(L, -2);
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushnil(L);
        setmetatable(L, -2);
        lua_pushstring(L, e.what());
        return lua_error(L);
    }
    return 1;
}

static int regex_match(lua_State* L)
{
    lua_settop(L, 3);
    auto& vm_ctx = get_vm_context(L);
    rawgetp(L, LUA_REGISTRYINDEX, &byte_span_mt_key);
    constexpr int byte_span_mt_idx = 4;

    auto regex = static_cast<std::regex*>(lua_touserdata(L, 1));
    if (!regex || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &regex_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    std::string_view str;
    byte_span_handle* src_bs;

    switch (lua_type(L, 2)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    case LUA_TSTRING:
        str = tostringview(L, 2);
        src_bs = nullptr;
        break;
    case LUA_TUSERDATA: {
        if (!lua_getmetatable(L, 2) || !lua_rawequal(L, -1, byte_span_mt_idx)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        src_bs = static_cast<byte_span_handle*>(lua_touserdata(L, 2));
        str = std::string_view(
            reinterpret_cast<char*>(src_bs->data.get()), src_bs->size);
    }
    }

    std::regex_constants::match_flag_type flags =
        std::regex_constants::match_default;

    switch (lua_type(L, 3)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    case LUA_TNIL:
        break;
    case LUA_TNUMBER:
        flags = static_cast<std::regex_constants::match_flag_type>(
            lua_tointeger(L, 3));
    }

    if (regex->mark_count() == 0) {
        auto match = std::regex_match(
            str.data(), str.data() + str.size(), *regex, flags);
        if (!match) {
            lua_pushnil(L);
            return 1;
        }

        lua_pushvalue(L, 2);
        return 1;
    }

    std::cmatch results;
    std::regex_match(str.data(), str.data() + str.size(), results, *regex,
                     flags);
    if (results.empty()) {
        lua_pushnil(L);
        return 1;
    }

    if (!lua_checkstack(
        L, results.size() - 1 + /*extra slots to create byte spans=*/1
    )) {
        vm_ctx.notify_errmem();
        return lua_yield(L, 0);
    }

    if (src_bs) {
        for (std::cmatch::size_type i = 1 ; i != results.size() ; ++i) {
            auto new_bs = static_cast<byte_span_handle*>(
                lua_newuserdata(L, sizeof(byte_span_handle))
            );
            lua_pushvalue(L, byte_span_mt_idx);
            setmetatable(L, -2);

            if (!results[i].matched) {
                new (new_bs) byte_span_handle{nullptr, 0, 0};
                continue;
            }

            std::shared_ptr<unsigned char[]> new_data(
                src_bs->data,
                reinterpret_cast<unsigned char*>(const_cast<char*>(
                    results[i].first)));
            new (new_bs) byte_span_handle{
                std::move(new_data),
                results[i].length(),
                src_bs->capacity - (results[i].first - str.data())};
        }
    } else {
        for (std::cmatch::size_type i = 1 ; i != results.size() ; ++i) {
            if (!results[i].matched) {
                lua_pushliteral(L, "");
                continue;
            }

            lua_pushlstring(L, results[i].first, results[i].length());
        }
    }
    return results.size() - 1;
}

int regex_search(lua_State* L)
{
    lua_settop(L, 3);

    auto regex = static_cast<std::regex*>(lua_touserdata(L, 1));
    if (!regex || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &regex_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    std::string_view str;
    switch (lua_type(L, 2)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    case LUA_TSTRING:
        str = tostringview(L, 2);
        break;
    case LUA_TUSERDATA: {
        rawgetp(L, LUA_REGISTRYINDEX, &byte_span_mt_key);
        if (!lua_getmetatable(L, 2) || !lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        auto src_bs = static_cast<byte_span_handle*>(lua_touserdata(L, 2));
        str = std::string_view(
            reinterpret_cast<char*>(src_bs->data.get()), src_bs->size);
    }
    }

    std::regex_constants::match_flag_type flags =
        std::regex_constants::match_default;

    switch (lua_type(L, 3)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    case LUA_TNIL:
        break;
    case LUA_TNUMBER:
        flags = static_cast<std::regex_constants::match_flag_type>(
            lua_tointeger(L, 3));
    }

    std::cmatch results;
    std::regex_search(str.data(), str.data() + str.size(), results, *regex,
                      flags);

    lua_createtable(L, /*narr=*/results.size(), /*nrec=*/1);

    lua_pushliteral(L, "empty");
    lua_pushboolean(L, results.empty());
    lua_rawset(L, -3);

    if (results.empty())
        return 1;

    lua_pushliteral(L, "start");
    lua_pushliteral(L, "end_");
    for (std::cmatch::size_type i = 0 ; i != results.size() ; ++i) {
        if (!results[i].matched)
            continue;

        lua_createtable(L, /*narr=*/0, /*nrec=*/2);
        {
            lua_pushvalue(L, -1 -2);
            lua_pushinteger(L, 1 + (results[i].first - str.data()));
            lua_rawset(L, -3);

            lua_pushvalue(L, -1 -1);
            lua_pushinteger(L, results[i].second - str.data());
            lua_rawset(L, -3);
        }
        lua_rawseti(L, -4, i);
    }
    lua_pop(L, 2);

    return 1;
}

int regex_split(lua_State* L)
{
    lua_settop(L, 2);
    rawgetp(L, LUA_REGISTRYINDEX, &byte_span_mt_key);
    constexpr int byte_span_mt_idx = 3;

    auto regex = static_cast<std::regex*>(lua_touserdata(L, 1));
    if (!regex || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &regex_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    std::string_view str;
    byte_span_handle* src_bs;

    switch (lua_type(L, 2)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    case LUA_TSTRING:
        str = tostringview(L, 2);
        src_bs = nullptr;
        break;
    case LUA_TUSERDATA: {
        if (!lua_getmetatable(L, 2) || !lua_rawequal(L, -1, byte_span_mt_idx)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        src_bs = static_cast<byte_span_handle*>(lua_touserdata(L, 2));
        str = std::string_view(
            reinterpret_cast<char*>(src_bs->data.get()), src_bs->size);
    }
    }

    lua_newtable(L);
    std::cregex_token_iterator end,
        it{str.data(), str.data() + str.size(), *regex, -1,
           std::regex_constants::match_not_null};
    int nf = 0;

    if (/*dont_include_separators=*/regex->mark_count() == 0) {
        if (src_bs) {
            for (; it != end ; ++it) {
                auto new_bs = static_cast<byte_span_handle*>(
                    lua_newuserdata(L, sizeof(byte_span_handle))
                );
                lua_pushvalue(L, -1);
                lua_rawseti(L, -3, ++nf);

                lua_pushvalue(L, byte_span_mt_idx);
                setmetatable(L, -2);
                lua_pop(L, 1);

                std::shared_ptr<unsigned char[]> new_data(
                    src_bs->data,
                    reinterpret_cast<unsigned char*>(const_cast<char*>(
                        it->first)));
                new (new_bs) byte_span_handle{
                    std::move(new_data),
                    it->length(),
                    src_bs->capacity - (it->first - str.data())};
            }
        } else {
            for (; it != end ; ++it) {
                lua_pushlstring(L, it->first, it->length());
                lua_rawseti(L, -2, ++nf);
            }
        }
    } else {
        // TODO: Should act as in Python and return the separators as well. This
        // should cover functionality from GAWK's extension (`seps` parameter)
        // to the AWK's split() function as well.
        push(L, std::errc::not_supported);
        return lua_error(L);
    }

    return 1;
}

int regex_patsplit(lua_State* L)
{
    lua_settop(L, 2);
    rawgetp(L, LUA_REGISTRYINDEX, &byte_span_mt_key);
    constexpr int byte_span_mt_idx = 3;

    auto regex = static_cast<std::regex*>(lua_touserdata(L, 1));
    if (!regex || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &regex_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    std::string_view str;
    byte_span_handle* src_bs;

    switch (lua_type(L, 2)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    case LUA_TSTRING:
        str = tostringview(L, 2);
        src_bs = nullptr;
        break;
    case LUA_TUSERDATA: {
        if (!lua_getmetatable(L, 2) || !lua_rawequal(L, -1, byte_span_mt_idx)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        src_bs = static_cast<byte_span_handle*>(lua_touserdata(L, 2));
        str = std::string_view(
            reinterpret_cast<char*>(src_bs->data.get()), src_bs->size);
    }
    }

    lua_newtable(L);
    auto it = str.data(), end = str.data() + str.size();
    std::cmatch results;
    bool matched = std::regex_search(it, end, results, *regex);
    auto flags = std::regex_constants::match_default;
    int nf = 0;

    if (/*dont_include_separators=*/regex->mark_count() == 0) {
        if (src_bs) {
            while (matched) {
                auto new_bs = static_cast<byte_span_handle*>(
                    lua_newuserdata(L, sizeof(byte_span_handle))
                );
                lua_pushvalue(L, -1);
                lua_rawseti(L, -3, ++nf);

                lua_pushvalue(L, byte_span_mt_idx);
                setmetatable(L, -2);
                lua_pop(L, 1);

                std::shared_ptr<unsigned char[]> new_data(
                    src_bs->data,
                    reinterpret_cast<unsigned char*>(const_cast<char*>(
                        results[0].first)));
                new (new_bs) byte_span_handle{
                    std::move(new_data),
                    results[0].length(),
                    src_bs->capacity - (results[0].first - str.data())};

                it = results[0].second;
                if (it == end)
                    break;
                if (
                    results[0].length() == 0 &&
                    std::regex_search(
                        it, end, results, *regex,
                        flags |
                        std::regex_constants::match_not_null |
                        std::regex_constants::match_continuous
                    )
                ) {
                    continue;
                }
                ++it;
                flags |= std::regex_constants::match_prev_avail;

                matched = std::regex_search(it, end, results, *regex, flags);
            }
        } else {
            while (matched) {
                lua_pushlstring(L, results[0].first, results[0].length());
                lua_rawseti(L, -2, ++nf);

                it = results[0].second;
                if (it == end)
                    break;
                if (
                    results[0].length() == 0 &&
                    std::regex_search(
                        it, end, results, *regex,
                        flags |
                        std::regex_constants::match_not_null |
                        std::regex_constants::match_continuous
                    )
                ) {
                    continue;
                }
                ++it;
                flags |= std::regex_constants::match_prev_avail;

                matched = std::regex_search(it, end, results, *regex, flags);
            }
        }
    } else {
        // TODO: Should act as in Python and return the separators as well. This
        // should cover functionality from GAWK's patsplit() function as well.
        push(L, std::errc::not_supported);
        return lua_error(L);
    }

    return 1;
}

inline int regex_mark_count(lua_State* L)
{
    auto regex = static_cast<std::regex*>(lua_touserdata(L, 1));
    lua_pushinteger(L, regex->mark_count());
    return 1;
}

inline int regex_grammar(lua_State* L)
{
    auto flags = static_cast<std::regex*>(lua_touserdata(L, 1))->flags();
    if (flags & std::regex_constants::ECMAScript) {
        lua_pushliteral(L, "ecma");
    } else if (flags & std::regex_constants::basic) {
        lua_pushliteral(L, "basic");
    } else {
        assert(flags & std::regex_constants::extended);
        lua_pushliteral(L, "extended");
    }
    return 1;
}

inline int regex_ignore_case(lua_State* L)
{
    auto flags = static_cast<std::regex*>(lua_touserdata(L, 1))->flags();
    lua_pushboolean(L, (flags & std::regex_constants::icase) ? 1 : 0);
    return 1;
}

inline int regex_nosubs(lua_State* L)
{
    auto flags = static_cast<std::regex*>(lua_touserdata(L, 1))->flags();
    lua_pushboolean(L, (flags & std::regex_constants::nosubs) ? 1 : 0);
    return 1;
}

inline int regex_optimized(lua_State* L)
{
    auto flags = static_cast<std::regex*>(lua_touserdata(L, 1))->flags();
    lua_pushboolean(L, (flags & std::regex_constants::optimize) ? 1 : 0);
    return 1;
}

static int regex_mt_index(lua_State* L)
{
    return dispatch_table::dispatch(
        hana::make_tuple(
            hana::make_pair(BOOST_HANA_STRING("mark_count"), regex_mark_count),
            hana::make_pair(BOOST_HANA_STRING("grammar"), regex_grammar),
            hana::make_pair(
                BOOST_HANA_STRING("ignore_case"), regex_ignore_case),
            hana::make_pair(BOOST_HANA_STRING("nosubs"), regex_nosubs),
            hana::make_pair(BOOST_HANA_STRING("optimized"), regex_optimized)
        ),
        [](std::string_view /*key*/, lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        },
        tostringview(L, 2),
        L
    );
}

void init_regex(lua_State* L)
{
    lua_pushlightuserdata(L, &regex_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/6);

        lua_pushliteral(L, "new");
        lua_pushcfunction(L, regex_new);
        lua_rawset(L, -3);

        lua_pushliteral(L, "match");
        lua_pushcfunction(L, regex_match);
        lua_rawset(L, -3);

        lua_pushliteral(L, "search");
        lua_pushcfunction(L, regex_search);
        lua_rawset(L, -3);

        lua_pushliteral(L, "split");
        lua_pushcfunction(L, regex_split);
        lua_rawset(L, -3);

        lua_pushliteral(L, "patsplit");
        lua_pushcfunction(L, regex_patsplit);
        lua_rawset(L, -3);

        lua_pushliteral(L, "match_flag");
        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/7);

            lua_pushliteral(L, "not_bol");
            lua_pushinteger(L, std::regex_constants::match_not_bol);
            lua_rawset(L, -3);

            lua_pushliteral(L, "not_eol");
            lua_pushinteger(L, std::regex_constants::match_not_eol);
            lua_rawset(L, -3);

            lua_pushliteral(L, "not_bow");
            lua_pushinteger(L, std::regex_constants::match_not_bow);
            lua_rawset(L, -3);

            lua_pushliteral(L, "not_eow");
            lua_pushinteger(L, std::regex_constants::match_not_eow);
            lua_rawset(L, -3);

            lua_pushliteral(L, "any");
            lua_pushinteger(L, std::regex_constants::match_any);
            lua_rawset(L, -3);

            lua_pushliteral(L, "not_null");
            lua_pushinteger(L, std::regex_constants::match_not_null);
            lua_rawset(L, -3);

            lua_pushliteral(L, "continuous");
            lua_pushinteger(L, std::regex_constants::match_continuous);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &regex_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "regex");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, regex_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<std::regex>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);
}

class regex_category_impl: public std::error_category
{
public:
    const char* name() const noexcept override;
    std::string message(int value) const noexcept override;
};

const char* regex_category_impl::name() const noexcept
{
    return "regex";
}

std::string regex_category_impl::message(int value) const noexcept
{
    switch (value) {
    case static_cast<int>(std::regex_constants::error_collate):
        return "the expression contains an invalid collating element name";
    case static_cast<int>(std::regex_constants::error_ctype):
        return "the expression contains an invalid character class name";
    case static_cast<int>(std::regex_constants::error_escape):
        return "the expression contains an invalid escaped character or a trailing escape";
    case static_cast<int>(std::regex_constants::error_backref):
        return "the expression contains an invalid back reference";
    case static_cast<int>(std::regex_constants::error_brack):
        return "the expression contains mismatched square brackets ('[' and ']')";
    case static_cast<int>(std::regex_constants::error_paren):
        return "the expression contains mismatched parentheses ('(' and ')')";
    case static_cast<int>(std::regex_constants::error_brace):
        return "the expression contains mismatched curly braces ('{' and '}')";
    case static_cast<int>(std::regex_constants::error_badbrace):
        return "the expression contains an invalid range in a {} expression";
    case static_cast<int>(std::regex_constants::error_range):
        return "the expression contains an invalid character range (e.g. [b-a])";
    case static_cast<int>(std::regex_constants::error_space):
        return "there was not enough memory to convert the expression into a finite state machine";
    case static_cast<int>(std::regex_constants::error_badrepeat):
        return "one of *?+{ was not preceded by a valid regular expression";
    case static_cast<int>(std::regex_constants::error_complexity):
        return "the complexity of an attempted match exceeded a predefined level";
    case static_cast<int>(std::regex_constants::error_stack):
        return "there was not enough memory to perform a match";
    default:
        return {};
    }
}

const std::error_category& regex_category() noexcept
{
    static regex_category_impl cat;
    return cat;
}

} // namespace emilua
