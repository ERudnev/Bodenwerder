#include <base/testing/runner.h>

#include <vector>

#define IQSM_TESTS(X) \
    X(dual_layer_model) \
    // end

// Workshop: верни каталог test/workshop/ целиком и раскомментируй блок ниже + group "Workshop" в groups.
//#define IQSM_WORKSHOP_TESTS(X) \
//    X(linear_commits) \
//    // end

BASETEST_FORWARD_DECLARE_TESTS(IQSM_TESTS)
//BASETEST_FORWARD_DECLARE_TESTS(IQSM_WORKSHOP_TESTS)

int main() {
    struct group final {
        const char* name = "";
        std::vector<base::testing::test_case> tests{};
    };

    const std::vector<group> groups{
        group{ "all", BASETEST_MAKE_LIST_TESTS(IQSM_TESTS) },
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
        BASETEST_LIST(BASETEST_NAMED("all", &tests::dual_layer_model)));
        //BASETEST_LIST(BASETEST_NAMED("performance_lab2", &tests::immutable_containers_perf2)));
    return s.ok() ? 0 : 1;
}