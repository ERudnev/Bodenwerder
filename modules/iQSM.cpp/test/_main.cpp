#include <base/testing/runner.h>

#define IQSM_TESTS(X) \
    X(typenames_varph) \
    X(schema_aspects) \
    X(worlddata) \
    X(delta_merge) \
    X(transient_usage) \
    X(validation_anchors) \
    X(caching_components) \
    X(validation_cache) \
    X(validation_existence) \
    X(transaction_repo) \
    X(globals) \
    X(schema) \
    X(resources) \
    // end

BASETEST_FORWARD_DECLARE_TESTS(IQSM_TESTS)

int main() {
    auto all_tests = BASETEST_MAKE_LIST_TESTS(IQSM_TESTS);

    return base::testing::run_tests(all_tests);
    /*return base::testing::run_tests(BASETEST_LIST(
        BASETEST_NAMED("transaction_repo", &tests::transaction_repo)
    ));*/
}