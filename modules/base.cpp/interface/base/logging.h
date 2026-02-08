#pragma once

#include <chrono>
#include <concepts>
#include <exception>
#include <iostream>
#include <format>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

// infra:
#define _INCOMPLETE_ throw ::base::detail::make_incomplete_message(__FILE__, __LINE__, NOTECS_FUNCTION_NAME)

namespace base {

    using Time = std::chrono::system_clock::time_point;

    Time now();
    std::string to_string(Time t);

    // --- Messaging
    inline void message(std::string_view msg) {
        std::cout << msg << std::endl;
    }

    inline void message(const char* msg) {
        std::cout << msg << std::endl;
    }

    inline void message(const std::string& msg) {
        message(msg.c_str());
    }

    // Minimal logging primitive. Compatible with std::format syntax.
    // Intentionally header-only and state-free.
    template <typename... Args>
    requires (sizeof...(Args) > 0)
    inline void message(std::format_string<Args...> fmt, Args&&... args) {
        std::cout << std::format(fmt, std::forward<Args>(args)...) << std::endl;
    }

    // --- Report helpers (handy for diagnostics)
    template<typename T>
    concept has_ostream_operator = requires(std::ostream& s, T value) {
        { s << value } -> std::convertible_to<std::ostream&>;
    };

    template<typename T>
    concept has_to_string_method = requires(T value) {
        { value.to_string() } -> std::convertible_to<std::string>;
    };

    template<typename T>
    concept has_to_string_external = requires(T value) {
        { to_string(value) } -> std::convertible_to<std::string>;
    };

    template<typename T>
    std::string report(const T&) {
        return "default report() impl.";
    }

    template<>
    inline std::string report<std::string>(const std::string& value) {
        return value;
    }

    template<has_ostream_operator T>
    std::string report(const T& value) {
        std::stringstream ss;
        ss << value;
        return "concept is passed: " + ss.str();
    };

    template<has_to_string_method T>
    std::string report(const T& value) {
        return value.to_string();
    }

    template<has_to_string_external T>
    std::string report(const T& value) {
        return to_string(value);
    };

    template<typename T>
    std::string to_string(const std::optional<T>& opt_val) {
        return opt_val ? report(opt_val.value()) : "{x}";
    }

    // --- Fatal / checks
    [[noreturn]] inline void fatal(std::string_view msg) {
        message("[{}] FATAL: {}", to_string(now()), msg);
        std::terminate();
    }

    template <typename... Args>
    requires (sizeof...(Args) > 0)
    [[noreturn]] inline void fatal(std::format_string<Args...> fmt, Args&&... args) {
        try {
            const auto rendered = std::format(fmt, std::forward<Args>(args)...);
            fatal(rendered);
        } catch (const std::format_error& e) {
            fatal(std::string("format error: ") + e.what());
        } catch (...) {
            fatal("unknown formatting error");
        }
    }

    inline void check(bool condition, const std::string& msg) {
        if (!condition) { fatal(msg); }
    }

} // namespace base

// infra / internal details (kept out of `base::` surface)
namespace base::detail {
    inline std::string make_incomplete_message(const char* file, int line, const char* function) {
        return std::string("INCOMPLETE at ") + file + ":" + std::to_string(line) + " in " + function;
    }
}
