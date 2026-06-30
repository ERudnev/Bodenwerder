#include <base/testing/runner.h>

#include <vector>

#define FQSM_TESTS(X) \
    X(flat_model_assembly) \
    // end

#define FQSM_FEATURES_TESTS(X) \
    X(structural_constraints) \
    X(custom_reactions) \
    X(killing_feature) \
    // end

#define FQSM_LOW_LEVEL_TESTS(X) \
    X(connections) \
    X(containers_updated) \
    X(delta_iterators) \
    X(globals) \
    X(immediate) \
    X(manipulation) \
    X(transaction_hierarchy) \
    // end

#define FQSM_MINIMODEL_TESTS(X) \
    // end

#define FQSM_Q1RUNTIME_TESTS(X) \
    X(schema_world_from_etalon) \
    // end

// Workshop: restore test/workshop/ in full, then uncomment the block below and add group "workshop" to groups.
//#define FQSM_WORKSHOP_TESTS(X) \
//    X(linear_commits) \
//    // end

BASETEST_FORWARD_DECLARE_TESTS(FQSM_TESTS)
BASETEST_FORWARD_DECLARE_TESTS(FQSM_FEATURES_TESTS)
BASETEST_FORWARD_DECLARE_TESTS(FQSM_LOW_LEVEL_TESTS)
BASETEST_FORWARD_DECLARE_TESTS(FQSM_MINIMODEL_TESTS)
BASETEST_FORWARD_DECLARE_TESTS(FQSM_Q1RUNTIME_TESTS)
//BASETEST_FORWARD_DECLARE_TESTS(FQSM_WORKSHOP_TESTS)

int main() {
    struct group final {
        const char* name = "";
        std::vector<base::testing::test_case> tests{};
    };

    const std::vector<group> groups{
        group{ "all", BASETEST_MAKE_LIST_TESTS(FQSM_TESTS) },
        group{ "features", BASETEST_MAKE_LIST_TESTS(FQSM_FEATURES_TESTS) },
        group{ "low_level", BASETEST_MAKE_LIST_TESTS(FQSM_LOW_LEVEL_TESTS) },
        group{ "minimodel", BASETEST_MAKE_LIST_TESTS(FQSM_MINIMODEL_TESTS) },
        group{ "q1runtime", BASETEST_MAKE_LIST_TESTS(FQSM_Q1RUNTIME_TESTS) },
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
        BASETEST_LIST(BASETEST_NAMED("selected", &tests::killing_feature)));
    return s.ok() ? 0 : 1;
}
