#include <base/testing/runner.h>

#include <vector>

#define BASE_TESTS(X) \
    X(smoke) \
    X(relations) \
    X(denseTable) \
    // end

BASETEST_FORWARD_DECLARE_TESTS(BASE_TESTS)

int main() {
    struct group final {
        const char* name = "";
        std::vector<base::testing::test_case> tests{};
    };

    const std::vector<group> groups{
        group{ "all", BASETEST_MAKE_LIST_TESTS(BASE_TESTS) },
    };

    base::testing::run_summary total{};

    for (std::size_t i = 0; i < groups.size(); ++i) {
        if (i != 0) base::message("");
        base::message(std::format("{}:", groups[i].name));

        const auto s = base::testing::run_tests(groups[i].tests);
        total += s;
    }

    base::message("");
    base::message(std::format("TOTAL SUMMARY: {} passed={}, failed={}, total={}",
        total.ok() ? "OK" : "FAIL",
        total.passed,
        total.failed,
        total.total()
    ));
    base::message(std::format("TOTAL TIME: {:.3f} ms", total.elapsed_ms()));

    return total.ok() ? 0 : 1;
}

int main_one_test() {
    const auto s = base::testing::run_tests(
        BASETEST_LIST(BASETEST_NAMED("all", &tests::smoke)));
    return s.ok() ? 0 : 1;
}
