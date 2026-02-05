#pragma once

#include <concepts>
#include <format>
#include <iostream>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <exception>
#include <iQSM/q1builtins.h>

namespace iqsm::logger {

  iqsm::q1::Time now();
  std::string to_string(iqsm::q1::Time t);

  inline void message(std::string_view msg) {
    std::cout << msg << std::endl;
  }

  // Minimal logging primitive. Compatible with std::format syntax.
  // Intentionally header-only and state-free.
  template <typename... Args>
  requires (sizeof...(Args) > 0)
  inline void message(std::format_string<Args...> fmt, Args&&... args) {
    std::cout << std::format(fmt, std::forward<Args>(args)...) << std::endl;
  }

  template<typename T>
  concept has_ostream_operator = requires(std::ostream& s, T value) {
    {s << value} -> std::convertible_to<std::ostream&>;
  };

  template<typename T>
  concept has_to_string_method = requires(T value) {
    {value.to_string()} -> std::convertible_to<std::string>;
  };

  template<typename T>
  concept has_to_string_external = requires(T value) {
    {to_string(value)} -> std::convertible_to<std::string>;
  };

  template<typename T>
  std::string report(const T& value) {
    return "default report() impl.";
  }

  // Explicit specialization for std::string
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

   // temp:
  template<typename T>
  std::string to_string(const std::optional<T>& opt_val) {
    return opt_val ? report(opt_val.value()) : "{x}";
  }

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
  
  // Checks
  inline void check(bool condition, const std::string& message) {
    if (!condition) { fatal(message); }
  }
}