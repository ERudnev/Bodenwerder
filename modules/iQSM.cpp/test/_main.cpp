#include <base/testing/runner.h>

#define IQSM_TESTS(X) \
    X(typenames_atomic) \
    X(dagdata_define) \
    X(worlddata_construct) \
    X(worlddata_unclosed_basis_returns_null) \
    X(delta_merge) \
    X(simpleworld_evolution) \
    X(validation_anchor) \
    // end

BASETEST_FORWARD_DECLARE_TESTS(IQSM_TESTS)

int main() {
    auto all_tests = BASETEST_MAKE_LIST_TESTS(IQSM_TESTS);

    return base::testing::run_tests(all_tests);
}