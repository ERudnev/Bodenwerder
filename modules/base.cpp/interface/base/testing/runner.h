#pragma once

#include <base/logging.h>
#include <base/testing/macros.h>

#include <exception>
#include <format>
#include <vector>

namespace base::testing {
    struct test_case final {
        const char* name;
        void (*fn)();
    };

    inline int run_tests(const std::vector<test_case>& tests) {
        int passed = 0;
        int failed = 0;

        for (const auto& t : tests) {
            base::message(std::format("[{}] started...", t.name));
            try {
                t.fn();
                ++passed;
                base::message(std::format("[{}] OK", t.name));
            } catch (const ::testing::failure& e) {
                ++failed;
                base::message(std::format("[{}] FAIL: {}", t.name, e.what()));
            } catch (const std::exception& e) {
                ++failed;
                base::message(std::format("[{}] EXCEPTION: {}", t.name, e.what()));
            } catch (...) {
                ++failed;
                base::message(std::format("[{}] EXCEPTION: <unknown>", t.name));
            }
            base::message("");
        }

        base::message(std::format("SUMMARY: {} passed={}, failed={}, total={}",
            failed == 0 ? "PASS" : "FAIL",
            passed, failed, passed + failed
        ));

        return failed == 0 ? 0 : 1;
    }
}

#define BASETEST_TEST(func) ::base::testing::test_case{#func, &func}
#define BASETEST_LIST(...) std::vector<::base::testing::test_case>{__VA_ARGS__}
#define BASETEST_NAMED(name_literal, fn_ptr) ::base::testing::test_case{name_literal, fn_ptr}

// X-macro helpers:
// - LIST_MACRO(X) is expected to call X(name) for each test name (unqualified).
#define BASETEST__FWD_ITEM(name) void name();

// Concrete helpers for the common namespace `tests` (keeps code small and predictable).
#define BASETEST_FORWARD_DECLARE_TESTS(LIST_MACRO) \
    namespace tests { LIST_MACRO(BASETEST__FWD_ITEM) }
#define BASETEST_MAKE_LIST_TESTS(LIST_MACRO) \
    ::BASETEST_LIST(LIST_MACRO(BASETEST__TESTS_ITEM))
#define BASETEST__TESTS_ITEM(name) BASETEST_NAMED(#name, &tests::name),


