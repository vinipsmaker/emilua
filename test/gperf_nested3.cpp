EMILUA_GPERF_DECLS_BEGIN(includes)
#include "tap_test.hpp"
#include <optional>
EMILUA_GPERF_DECLS_END(includes)

EMILUA_GPERF_DECLS_BEGIN(impl)
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
EMILUA_GPERF_DECLS_END(impl)

EMILUA_GPERF_DECLS_BEGIN(impl)
EMILUA_GPERF_NAMESPACE(emilua)
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
EMILUA_GPERF_DECLS_END(impl)

int main()
{
    tap_test t;

    // with default value
    EMILUA_TEST_EQUAL(t, emilua::str2int("first"), 1);
    EMILUA_TEST_EQUAL(t, emilua::str2int("second"), 2);
    EMILUA_TEST_EQUAL(t, emilua::str2int("third"), 3);
    EMILUA_TEST_EQUAL(t, emilua::str2int("noop"), -1);
}
