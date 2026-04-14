#include <base/testing/runner.h>

#include <vector>

#define IQSM_TESTS(X) \
    /* X(typenames_varph) */ \
    X(schema_aspects) \
    /* X(worlddata) */ \
    /* X(delta_merge) */ \
    /* X(validation_anchors) */ \
    /* X(caching_components) */ \
    /* X(validation_cache) */ \
    /* X(validation_existence) */ \
    /* X(transaction_repo) */ \
    /* X(globals) */ \
    /* X(handles) */ \
    X(resource_example) \
    /* X(immutable_containers_perf) */ \
    X(model_is_compileable) \
    X(complex_constructor) \
    X(ownership) \
    X(multistate_system) \
    X(mass_operations) \
    // end

#define IQSM_KNOWN_ISSUES_TESTS(X) \
    X(merge_add_remove_constructor_issue) \
    // end

#define IQSM_VALIDATION_TESTS(X) \
    X(validation_placeholder) \
    X(validation_caching_component) \
    X(validation_tag_globals) \
    // end

#define IQSM_OPERATIONS_TESTS(X) \
    X(transaction_repo) \
    X(globals) \
    // end

BASETEST_FORWARD_DECLARE_TESTS(IQSM_TESTS)
BASETEST_FORWARD_DECLARE_TESTS(IQSM_VALIDATION_TESTS)
BASETEST_FORWARD_DECLARE_TESTS(IQSM_OPERATIONS_TESTS)
BASETEST_FORWARD_DECLARE_TESTS(IQSM_KNOWN_ISSUES_TESTS)

int main() {
    struct group final {
        const char* name = "";
        std::vector<base::testing::test_case> tests{};
    };

    const std::vector<group> groups{
        group{ "all", BASETEST_MAKE_LIST_TESTS(IQSM_TESTS) },
        group{ "validation", BASETEST_MAKE_LIST_TESTS(IQSM_VALIDATION_TESTS) },
        group{ "operations", BASETEST_MAKE_LIST_TESTS(IQSM_OPERATIONS_TESTS) },
        //group{ "known_issues", BASETEST_MAKE_LIST_TESTS(IQSM_KNOWN_ISSUES_TESTS) },
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
    /*return base::testing::run_tests(BASETEST_LIST(
        BASETEST_NAMED("transaction_repo", &tests::transaction_repo)
    ));*/
}