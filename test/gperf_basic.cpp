#include "tap_test.hpp"
#include <optional>
#include <variant>

#include <fmt/std.h>

namespace emilua::gperf::detail {
template<class T>
auto value_or(const T* p, decltype(std::declval<T>().action) default_value)
    -> decltype(default_value)
{
    return p ? p->action : default_value;
}

template<class T>
auto make_optional(const T* p)
    -> std::optional<decltype(std::declval<T>().action)>
{
    if (p)
        return p->action;
    else
        return std::nullopt;
}
} // namespace emilua::gperf::detail

int str2int(std::string_view s)
{
    return EMILUA_GPERF_BEGIN(s)
        EMILUA_GPERF_PARAM(int action)
        EMILUA_GPERF_DEFAULT_VALUE(-1)
        EMILUA_GPERF_PAIR("first", 1)
        EMILUA_GPERF_PAIR("second", 2)
        EMILUA_GPERF_PAIR("third", 3)
    EMILUA_GPERF_END(s);
}

std::variant<std::monostate, int> str2int2(std::string_view s)
{
    auto v = EMILUA_GPERF_BEGIN(s)
        EMILUA_GPERF_PARAM(int action)
        EMILUA_GPERF_PAIR("first", 1)
        EMILUA_GPERF_PAIR("second", 2)
        EMILUA_GPERF_PAIR("third", 3)
    EMILUA_GPERF_END(s);
    if (v)
        return *v;
    else
        return std::monostate{};
}

template<class T>
std::variant<std::monostate, T> tovariant(T v)
{
    return v;
}

int str2int3(std::string_view s)
{
    return EMILUA_GPERF_BEGIN(s)
        EMILUA_GPERF_PARAM(int action)
        EMILUA_GPERF_DEFAULT_VALUE(-1)
        EMILUA_GPERF_PAIR(
            "first",
#if 1
            1
#endif // 1
        )
        EMILUA_GPERF_PAIR(
            "second",
#if 1
            2
#endif // 1
        )
        EMILUA_GPERF_PAIR(
            "third",
#if 1
            3 // some comment
#endif // 1
        )
    EMILUA_GPERF_END(s);
}

int main()
{
    tap_test t;

    // with default value
    EMILUA_TEST_EQUAL(t, str2int("first"), 1);
    EMILUA_TEST_EQUAL(t, str2int("second"), 2);
    EMILUA_TEST_EQUAL(t, str2int("third"), 3);
    EMILUA_TEST_EQUAL(t, str2int("noop"), -1);

    // w/o default value
    EMILUA_TEST_EQUAL(t, str2int2("first"), tovariant(1));
    EMILUA_TEST_EQUAL(t, str2int2("second"), tovariant(2));
    EMILUA_TEST_EQUAL(t, str2int2("third"), tovariant(3));
    EMILUA_TEST_EQUAL(t, str2int2("noop"),
                      (std::variant<std::monostate, int>{}));

    // multiline
    EMILUA_TEST_EQUAL(t, str2int3("first"), 1);
    EMILUA_TEST_EQUAL(t, str2int3("second"), 2);
    EMILUA_TEST_EQUAL(t, str2int3("third"), 3);
    EMILUA_TEST_EQUAL(t, str2int3("noop"), -1);
}
