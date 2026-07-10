#include <base/testing/runner.h>

#include <vector>

#define FQSM_FEATURES_TESTS(X) \
    X(structural_constraints) \
    X(anchor_constraints) \
    X(custom_reactions) \
    X(killing_feature) \
    X(group_category) \
    X(group_performance) \
    X(serialization) \
    X(destructor) \
    // end

#define FQSM_LOW_LEVEL_TESTS(X) \
    X(containers_updated) \
    X(delta_iterators) \
    X(globals) \
    X(global_is_change_too) \
    X(immediate) \
    X(transaction_hierarchy) \
    // end

#define FQSM_MINIMODEL_TESTS(X) \
    // end

#define FQSM_Q1RUNTIME_TESTS(X) \
    X(schema_world_from_etalon) \
    // end

#define FQSM_WORKSHOP_TESTS(X) \
    X(polymorphic_behavior_exp) \
    // end

BASETEST_FORWARD_DECLARE_TESTS(FQSM_FEATURES_TESTS)
BASETEST_FORWARD_DECLARE_TESTS(FQSM_LOW_LEVEL_TESTS)
BASETEST_FORWARD_DECLARE_TESTS(FQSM_MINIMODEL_TESTS)
BASETEST_FORWARD_DECLARE_TESTS(FQSM_Q1RUNTIME_TESTS)
BASETEST_FORWARD_DECLARE_TESTS(FQSM_WORKSHOP_TESTS)

int call_all_tests() {
    struct group final {
        const char* name = "";
        std::vector<base::testing::test_case> tests{};
    };

    const std::vector<group> groups{
        group{ "features", BASETEST_MAKE_LIST_TESTS(FQSM_FEATURES_TESTS) },
        group{ "low_level", BASETEST_MAKE_LIST_TESTS(FQSM_LOW_LEVEL_TESTS) },
        group{ "minimodel", BASETEST_MAKE_LIST_TESTS(FQSM_MINIMODEL_TESTS) },
        group{ "q1runtime", BASETEST_MAKE_LIST_TESTS(FQSM_Q1RUNTIME_TESTS) },
        group{ "workshop", BASETEST_MAKE_LIST_TESTS(FQSM_WORKSHOP_TESTS) },
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

int call_specific_test() {
    const auto s = base::testing::run_tests(
        BASETEST_LIST(BASETEST_NAMED("selected", &tests::polymorphic_behavior_exp)));
    return s.ok() ? 0 : 1;
}

int main() {
    //return call_specific_test();
    return call_all_tests();
};
