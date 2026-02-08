#pragma once

#include <memory>
#include <stdexcept>
#include <base/logging.h>
#include <base/testing/ScopedTimer.h>

namespace testing {

    void function_wrap(auto func, const std::string legend)
    {
        testing::scoped_timer timer("total");
        base::message(std::format("[{}] started...", legend));
        try {
            func();
        } catch (const std::exception& e) {
            base::message("...FAILED: exception caught: " + std::string(e.what()));
        } catch (...) {
            base::message("...FAILED: Unknown exception caught");
        }
        base::message(std::format("....DONE: [{}], {}", legend, timer.finish()));
        base::message("");
    }

    struct scope_announcement {
        scope_announcement(const char* name, bool always = false) { init(name, always); }
        scope_announcement(const std::string& name, bool always = false) { init(name.c_str(), always); }
        ~scope_announcement() {
            if (timer) {
                base::message(std::format("<<< scope finished, {}", timer->name(), timer->finish()));
            }
        }
    private:
        std::unique_ptr<testing::scoped_timer> timer;

        void init(const char* name, bool always) {
            bool do_report = always;
            #ifdef DEBUG
            do_report = true;
            #endif
            if (do_report) {
                timer = std::make_unique<testing::scoped_timer>(name);
                base::message(std::format(">>> scope [{}] announced...", timer->name()));
            }
        }
    };

    struct failure final : std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    [[noreturn]] inline void fail(const std::string& message) { throw failure(message); }
}

#define EXPECT_TRUE(condition) \
    if (!(condition)) { testing::fail(std::format("Expecting true, but got false: {} (at {}:{})", #condition, __FILE__, __LINE__)); }

#define EXPECT_EQ(a, b) \
    do { \
        auto _a_val = (a); \
        auto _b_val = (b); \
        if (_a_val != _b_val) { \
            testing::fail(std::format("Expecting equality, but got: {} != {} (values: {} != {}) (at {}:{})", #a, #b, _a_val, _b_val, __FILE__, __LINE__)); \
        } \
    } while(0)

#define ADD_FAILURE(message) \
    do { \
        testing::fail(std::format("Failure: {} (at {}:{})", message, __FILE__, __LINE__)); \
    } while(0)

#define RUN_TEST(func) testing::function_wrap(func, #func);
