#include <base/testing/runner.h>

#define IQSM_TESTS(X) \
    X(typenames_atomic) \
    X(schema_aspects) \
    X(worlddata_construct) \
    X(worlddata_closure_pulls_dependencies) \
    X(delta_merge) \
    X(validation_anchor) \
    X(schema) \
    X(molecular_analysis_system) \
    // end

BASETEST_FORWARD_DECLARE_TESTS(IQSM_TESTS)

int main() {
    auto all_tests = BASETEST_MAKE_LIST_TESTS(IQSM_TESTS);

    return base::testing::run_tests(all_tests);
}