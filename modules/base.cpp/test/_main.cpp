#include <base/testing/runner.h>

#include <vector>

#define BASE_TESTS(X) \
    X(smoke) \
    X(serialization_roundtrip) \
    // end

BASETEST_FORWARD_DECLARE_TESTS(BASE_TESTS)

int main() {
    const auto summary = base::testing::run_tests(BASETEST_MAKE_LIST_TESTS(BASE_TESTS));

    base::message("");
    base::message(std::format("TOTAL SUMMARY: {} passed={}, failed={}, total={}",
        summary.ok() ? "OK" : "FAIL",
        summary.passed,
        summary.failed,
        summary.total()
    ));
    base::message(std::format("TOTAL TIME: {:.3f} ms", summary.elapsed_ms()));

    return summary.ok() ? 0 : 1;
}
