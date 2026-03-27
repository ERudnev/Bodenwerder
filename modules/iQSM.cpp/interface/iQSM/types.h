#pragma once

#include <format>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeindex>

namespace iqsm::internals {
    struct Types {
        using RuntimeId = std::type_index;
        using StaticId = std::string;
    };

    template<typename T>
    constexpr std::string_view type_name(std::type_identity<T> = {});
}

// ---- implementation details
namespace iqsm::internals {
    constexpr std::string_view _strip_decorations(std::string_view n) {
        if (n.starts_with("struct ")) { n.remove_prefix(7); }
        else if (n.starts_with("class ")) { n.remove_prefix(6); }
        else if (n.starts_with("enum ")) { n.remove_prefix(5); }
        return n;
    }

    constexpr std::string_view _slice_between(std::string_view s, std::string_view left, std::string_view right) {
        const auto lpos = s.find(left);
        if (lpos == std::string_view::npos) return {};
        s.remove_prefix(lpos + left.size());
        const auto rpos = s.rfind(right);
        if (rpos == std::string_view::npos) return {};
        return s.substr(0, rpos);
    }

    // De-facto cross-compiler type name extraction:
    // - MSVC: __FUNCSIG__
    // - Clang/GCC: __PRETTY_FUNCTION__
    // Not standard, but stable enough for diagnostics/pretty logging.
    template<typename T>
    constexpr std::string_view type_name(std::type_identity<T>) {
#if defined(_MSC_VER)
        constexpr std::string_view sig = __FUNCSIG__;
        auto name = _slice_between(sig, "type_name<", ">(");
        return _strip_decorations(name);
#elif defined(__clang__) || defined(__GNUC__)
        constexpr std::string_view sig = __PRETTY_FUNCTION__;
        const auto pos = sig.find("T = ");
        if (pos == std::string_view::npos) return {};
        auto tail = sig.substr(pos + 4);
        const auto end = tail.find(']');
        if (end == std::string_view::npos) return {};
        return _strip_decorations(tail.substr(0, end));
#else
        return {};
#endif
    }
}

