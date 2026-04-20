#include <base/testing/runner.h>

#include <vector>

#define MQSM_TESTS(X) \
    X(smoke) \
    // end

#define MQSM_WORKSHOP_TESTS(X) \
    X(fields) \
    // end

BASETEST_FORWARD_DECLARE_TESTS(MQSM_TESTS)
BASETEST_FORWARD_DECLARE_TESTS(MQSM_WORKSHOP_TESTS)

int main() {
    struct group final {
        const char* name = "";
        std::vector<base::testing::test_case> tests{};
    };

    const std::vector<group> groups{
        group{ "all", BASETEST_MAKE_LIST_TESTS(MQSM_TESTS) },
        group{ "Workshop", BASETEST_MAKE_LIST_TESTS(MQSM_WORKSHOP_TESTS) },
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
