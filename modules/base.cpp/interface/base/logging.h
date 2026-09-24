#pragma once

#include <concepts>
#include <format>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

#ifndef NOTECS_FUNCTION_NAME
#define NOTECS_FUNCTION_NAME __func__
#endif

// infra:
#define _INCOMPLETE_ throw ::base::detail::IncompleteError(::base::detail::make_incomplete_message(__FILE__, __LINE__, NOTECS_FUNCTION_NAME))
#define _THROW_LOGIC_ERROR_ throw std::logic_error(::base::detail::make_logic_error_message(__FILE__, __LINE__, __func__))
#define _IMPLEMENT_ME_ static_assert(false, "needs some code to implement here")
#define _DEBUG_REPORT_ ::base::message("{}:{}: {}", __FILE__, __LINE__, NOTECS_FUNCTION_NAME)

namespace base {

    // --- Messaging
    void message(std::string_view msg);

    inline void message(const char* msg) {
        message(std::string_view{msg});
    }

    inline void message(const std::string& msg) {
        message(std::string_view{msg});
    }

    void vmessage(std::string_view fmt, std::format_args args);

    // Minimal logging primitive. Compatible with std::format syntax.
    template <typename... Args>
    requires (sizeof...(Args) > 0)
    inline void message(std::format_string<Args...> fmt, Args&&... args) {
        vmessage(fmt.get(), std::make_format_args(args...));
    }

    // One open line. tick/mark append '.' and flush; the destructor ends the line.
    struct Progress {
        static Progress* current;

        explicit Progress(std::string_view label);
        void tick();
        ~Progress();
        static void mark();
        static void markEvery(long long index);
    };

    // Dim / secondary chatter (ANSI bright-black). Same surface as message.
    void whisper(std::string_view msg);

    inline void whisper(const char* msg) {
        whisper(std::string_view{msg});
    }

    inline void whisper(const std::string& msg) {
        whisper(std::string_view{msg});
    }

    void vwhisper(std::string_view fmt, std::format_args args);

    template <typename... Args>
    requires (sizeof...(Args) > 0)
    inline void whisper(std::format_string<Args...> fmt, Args&&... args) {
        vwhisper(fmt.get(), std::make_format_args(args...));
    }

    // Warning (ANSI yellow). Same surface as message.
    void warning(std::string_view msg);

    inline void warning(const char* msg) {
        warning(std::string_view{msg});
    }

    inline void warning(const std::string& msg) {
        warning(std::string_view{msg});
    }

    void vwarning(std::string_view fmt, std::format_args args);

    template <typename... Args>
    requires (sizeof...(Args) > 0)
    inline void warning(std::format_string<Args...> fmt, Args&&... args) {
        vwarning(fmt.get(), std::make_format_args(args...));
    }

    // --- Report helpers (handy for diagnostics)
    template<typename T>
    concept has_to_string_external = requires(T value) {
        { to_string(value) } -> std::convertible_to<std::string>;
    };

    template<typename T>
    concept has_ostream_operator = requires(std::ostream& s, T value) {
        { s << value } -> std::convertible_to<std::ostream&>;
    } and not has_to_string_external<T>;

    template<typename T>
    concept has_to_string_method = requires(T value) {
        { value.to_string() } -> std::convertible_to<std::string>;
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
        return ss.str();
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
    [[noreturn]] void fatal(std::string_view msg);

    [[noreturn]] void vfatal(std::string_view fmt, std::format_args args);

    template <typename... Args>
    requires (sizeof...(Args) > 0)
    [[noreturn]] inline void fatal(std::format_string<Args...> fmt, Args&&... args) {
        vfatal(fmt.get(), std::make_format_args(args...));
    }

    inline void check(bool condition, const std::string& msg) {
        if (!condition) { fatal(msg); }
    }

} // namespace base

// infra / internal details (kept out of `base::` surface)
namespace base::detail {
    struct IncompleteError : std::logic_error {
        using std::logic_error::logic_error;
    };

    inline std::string make_incomplete_message(const char* file, int line, const char* function) {
        return std::string("INCOMPLETE: reached placeholder code. ")
            + "Location: " + file + ":" + std::to_string(line) + " in " + function;
    }

    inline std::string make_logic_error_message(const char* file, int line, const char* function) {
        return std::string("LOGIC ERROR at ") + file + ":" + std::to_string(line) + " in " + function;
    }
}
