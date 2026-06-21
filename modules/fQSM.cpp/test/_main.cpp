#include <base/testing/runner.h>

#include <vector>

#define FQSM_TESTS(X) \
    X(flat_model_assembly) \
    // end

#define FQSM_INTERNALS_TESTS(X) \
    X(delta_iterators) \
    X(immediate) \
    X(transaction_hierarchy) \
    X(manipulation) \
    X(globals) \
    X(connections) \
    X(containers_updated) \
    // end

#define FQSM_Q1RUNTIME_TESTS(X) \
    X(schema_world_from_etalon) \
    // end

#define FQSM_REACTIONS_TESTS(X) \
    X(component_norms) \
    X(attribute_norms) \
    X(killing_feature) \
    // end

// Workshop: верни каталог test/workshop/ целиком и раскомментируй блок ниже + group "Workshop" в groups.
//#define IQSM_WORKSHOP_TESTS(X) \
//    X(linear_commits) \
//    // end

BASETEST_FORWARD_DECLARE_TESTS(FQSM_TESTS)
BASETEST_FORWARD_DECLARE_TESTS(FQSM_INTERNALS_TESTS)
BASETEST_FORWARD_DECLARE_TESTS(FQSM_Q1RUNTIME_TESTS)
BASETEST_FORWARD_DECLARE_TESTS(FQSM_REACTIONS_TESTS)
//BASETEST_FORWARD_DECLARE_TESTS(IQSM_WORKSHOP_TESTS)

int main_all_test() {
    struct group final {
        const char* name = "";
        std::vector<base::testing::test_case> tests{};
    };

    const std::vector<group> groups{
        group{ "all", BASETEST_MAKE_LIST_TESTS(FQSM_TESTS) },
        group{ "internals", BASETEST_MAKE_LIST_TESTS(FQSM_INTERNALS_TESTS) },
        group{ "q1runtime", BASETEST_MAKE_LIST_TESTS(FQSM_Q1RUNTIME_TESTS) },
        group{ "reactions", BASETEST_MAKE_LIST_TESTS(FQSM_REACTIONS_TESTS) },
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

int main() {
    const auto s = base::testing::run_tests(
        BASETEST_LIST(BASETEST_NAMED("all", &tests::flat_model_assembly)));
        //BASETEST_LIST(BASETEST_NAMED("internals/dense_table_overlay", &tests::dense_table_overlay)));
    return s.ok() ? 0 : 1;
}