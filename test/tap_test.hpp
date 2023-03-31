#include <string_view>
#include <iostream>
#include <string>
#include <vector>

#include <fmt/format.h>

#define EMILUA_TEST(T, X) (T).test(X, #X)
#define EMILUA_TEST_EQUAL(T, A, B) (T).equal(A, B, #A, #B)

class tap_test
{
public:
    ~tap_test();

    void test(bool, std::string_view);

    template<class T1, class T2>
    void equal(const T1&, const T2&, std::string_view, std::string_view);

private:
    std::vector<std::string> messages;
};

void tap_test::test(bool b, std::string_view e)
{
    std::string m;
    if (b) {
        m = fmt::format("ok {} - {}\n", messages.size() + 1, e);
    } else {
        m = fmt::format("not ok {} - {} is false\n", messages.size() + 1, e);
    }
    messages.emplace_back(std::move(m));
}

template<class T1, class T2>
void tap_test::equal(const T1& a, const T2& b,
                     std::string_view e1, std::string_view e2)
{
    std::string m;
    if (a == b) {
        m = fmt::format(
            "ok {} - {} == {}\n",
            messages.size() + 1,
            e1, e2);
    } else {
        m = fmt::format(
            "not ok {} - {} != {} ({} != {})\n",
            messages.size() + 1,
            e1, e2, a, b);
    }
    messages.emplace_back(std::move(m));
}

tap_test::~tap_test()
{
    std::cout << "1.." << messages.size() << '\n';
    for (const auto& m : messages) {
        std::cout << m;
    }
}
