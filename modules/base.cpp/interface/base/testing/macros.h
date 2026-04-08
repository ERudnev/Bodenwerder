#pragma once

#include <memory>
#include <sstream>
#include <exception>
#include <stdexcept>
#include <string>
#include <utility>
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

    namespace detail {
        template<typename F>
        inline bool eval_bool(F&& f, const char* expr, const char* file, int line) {
            try {
                return static_cast<bool>(std::forward<F>(f)());
            } catch (const std::exception& e) {
                testing::fail(std::format(
                    "EXCEPTION while evaluating {}: {} (at {}:{})",
                    expr, e.what(), file, line));
            } catch (...) {
                testing::fail(std::format(
                    "EXCEPTION while evaluating {}: <unknown> (at {}:{})",
                    expr, file, line));
            }
        }

        struct Message final {
            std::ostringstream stream;

            template<typename T>
            Message& operator<<(T&& value) {
                stream << std::forward<T>(value);
                return *this;
            }

            std::string str() const { return stream.str(); }
        };

        struct FailHelper final {
            const char* expected;
            const char* expr;
            const char* file;
            int line;

            void operator=(const Message& msg) const {
                auto base_msg = std::format(
                    "Expecting {}, but got {}: {} (at {}:{})",
                    expected,
                    std::string(expected) == "true" ? "false" : "true",
                    expr,
                    file,
                    line);
                const auto extra = msg.str();
                if (!extra.empty()) {
                    base_msg.append(": ");
                    base_msg.append(extra);
                }
                testing::fail(base_msg);
            }
        };

        template<typename A, typename B>
        struct EqCapture final {
            const char* a_expr;
            const char* b_expr;
            const char* file;
            int line;

            std::decay_t<A> a;
            std::decay_t<B> b;

            EqCapture(const char* a_expr, const char* b_expr, const char* file, int line, A&& a, B&& b)
                : a_expr(a_expr)
                , b_expr(b_expr)
                , file(file)
                , line(line)
                , a(std::forward<A>(a))
                , b(std::forward<B>(b))
            {}

            [[nodiscard]] bool ok() const { return a == b; }

            struct Helper final {
                const EqCapture* cap;

                void operator=(const Message& msg) const {
                    auto base_msg = std::format(
                        "Expecting equality, but got: {} != {} (values: {} != {}) (at {}:{})",
                        cap->a_expr,
                        cap->b_expr,
                        base::report(cap->a),
                        base::report(cap->b),
                        cap->file,
                        cap->line);
                    const auto extra = msg.str();
                    if (!extra.empty()) {
                        base_msg.append(": ");
                        base_msg.append(extra);
                    }
                    testing::fail(base_msg);
                }
            };

            [[nodiscard]] Helper helper() const { return Helper{this}; }
        };

        template<typename A, typename B, typename FA, typename FB>
        inline EqCapture<A, B> make_eq_capture(
            const char* a_expr,
            const char* b_expr,
            const char* file,
            int line,
            FA&& a_eval,
            FB&& b_eval)
        {
            try {
                return EqCapture<A, B>{a_expr, b_expr, file, line,
                    std::forward<FA>(a_eval)(),
                    std::forward<FB>(b_eval)()};
            } catch (const std::exception& e) {
                testing::fail(std::format(
                    "EXCEPTION while evaluating EXPECT_EQ({}, {}): {} (at {}:{})",
                    a_expr, b_expr, e.what(), file, line));
            } catch (...) {
                testing::fail(std::format(
                    "EXCEPTION while evaluating EXPECT_EQ({}, {}): <unknown> (at {}:{})",
                    a_expr, b_expr, file, line));
            }
        }
    }
}

#define EXPECT_TRUE(condition) \
    switch (0) case 0: default: \
        if (::testing::detail::eval_bool([&] { return (condition); }, #condition, __FILE__, __LINE__)) ; \
        else ::testing::detail::FailHelper{"true", #condition, __FILE__, __LINE__} = ::testing::detail::Message()

#define EXPECT_FALSE(condition) \
    switch (0) case 0: default: \
        if (not ::testing::detail::eval_bool([&] { return (condition); }, #condition, __FILE__, __LINE__)) ; \
        else ::testing::detail::FailHelper{"false", #condition, __FILE__, __LINE__} = ::testing::detail::Message()

#define EXPECT_EQ(a, b) \
    switch (0) case 0: default: \
        if (const auto _cap = ::testing::detail::make_eq_capture<decltype((a)), decltype((b))>( \
                #a, #b, __FILE__, __LINE__, \
                [&]() -> decltype((a)) { return (a); }, \
                [&]() -> decltype((b)) { return (b); } \
            ); _cap.ok()) ; \
        else _cap.helper() = ::testing::detail::Message()

#define ADD_FAILURE(message) \
    do { \
        testing::fail(std::format("Failure: {} (at {}:{})", message, __FILE__, __LINE__)); \
    } while(0)

#define RUN_TEST(func) testing::function_wrap(func, #func);
