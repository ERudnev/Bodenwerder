#include <base/testing/runner.h>

#define IQSM_TESTS(X) \
    X(typenames_atomic) \
    X(dagdata_define) \
    X(always_succeeds) \
    // end

BASETEST_FORWARD_DECLARE_TESTS(IQSM_TESTS)

int main() {
    auto all_tests = BASETEST_MAKE_LIST_TESTS(IQSM_TESTS);

    return base::testing::run_tests(all_tests);
}