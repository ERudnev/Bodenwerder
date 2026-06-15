#include <base/testing/runner.h>

#include <vector>

#define CONTAINERS_TESTS(X) \
    X(tableDelta) \
    // end

#define CANNONBALL_TESTS(X) \
    X(cannonballSmoke) \
    X(denseTable) \
    X(delta_demonstration) \
    X(patch_operations) \
    X(patch_integrate) \
    X(patch_merge) \
    X(draft_patch_build) \
    X(preview_future_world) \
    // end

#define BASE_TESTS(X) \
    X(smoke) \
    CONTAINERS_TESTS(X) \
    CANNONBALL_TESTS(X) \
    // end

BASETEST_FORWARD_DECLARE_TESTS(BASE_TESTS)

int main() {
    struct group final {
        const char* name = "";
        std::vector<base::testing::test_case> tests{};
    };

    const std::vector<group> groups{
        group{ "containers", BASETEST_MAKE_LIST_TESTS(CONTAINERS_TESTS) },
        group{ "cannonball", BASETEST_LIST(
            BASETEST_NAMED("smoke", &tests::cannonballSmoke),
            BASETEST_NAMED("denseTable", &tests::denseTable),
            BASETEST_NAMED("delta_demonstration", &tests::delta_demonstration),
            BASETEST_NAMED("patch_operations", &tests::patch_operations),
            BASETEST_NAMED("patch_integrate", &tests::patch_integrate),
            BASETEST_NAMED("patch_merge", &tests::patch_merge),
            BASETEST_NAMED("draft_patch_build", &tests::draft_patch_build),
            BASETEST_NAMED("preview_future_world", &tests::preview_future_world)
        ) },
        group{ "all", BASETEST_MAKE_LIST_TESTS(BASE_TESTS) },
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
        BASETEST_LIST(BASETEST_NAMED("all", &tests::smoke)));
    return s.ok() ? 0 : 1;
}
