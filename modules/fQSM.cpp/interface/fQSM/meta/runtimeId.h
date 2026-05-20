#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeindex>
#include <utility>

#include <fQSM/meta/concepts.h>

namespace fqsm::meta::aspect {

    struct Rtid {
        std::type_index value;
    
        template<Any Meta>
        static Rtid of() {
            return Rtid{typeid(Meta)};
        }
    
        bool operator==(const Rtid&) const = default;
        bool operator<(const Rtid& other) const {
            const auto thisHash = value.hash_code();
            const auto otherHash = other.value.hash_code();
            if (thisHash != otherHash) return thisHash < otherHash;
            return std::string_view{value.name()} < std::string_view{other.value.name()};
        }

        struct Hash {
            auto operator()(const Rtid& id) const -> std::size_t {
                return id.value.hash_code();
            }
        };
    
    private:
        explicit Rtid(std::type_index value) : value(value) {}
    };

    namespace detail {
        constexpr std::string_view strip_decorations(std::string_view name) {
            if (name.starts_with("struct ")) { name.remove_prefix(7); }
            else if (name.starts_with("class ")) { name.remove_prefix(6); }
            else if (name.starts_with("enum ")) { name.remove_prefix(5); }
            return name;
        }

        constexpr std::string_view slice_between(std::string_view value, std::string_view left, std::string_view right) {
            const auto leftPos = value.find(left);
            if (leftPos == std::string_view::npos) return {};
            value.remove_prefix(leftPos + left.size());

            const auto rightPos = value.rfind(right);
            if (rightPos == std::string_view::npos) return {};
            return value.substr(0, rightPos);
        }

        template<typename T>
        constexpr std::string_view type_name(std::type_identity<T> = {}) {
#if defined(_MSC_VER)
            constexpr std::string_view sig = __FUNCSIG__;
            auto name = slice_between(sig, "type_name<", ">(");
            return strip_decorations(name);
#elif defined(__clang__) || defined(__GNUC__)
            constexpr std::string_view sig = __PRETTY_FUNCTION__;
            const auto pos = sig.find("T = ");
            if (pos == std::string_view::npos) return {};
            auto tail = sig.substr(pos + 4);
            const auto end = tail.find(']');
            if (end == std::string_view::npos) return {};
            return strip_decorations(tail.substr(0, end));
#else
            return {};
#endif
        }
    }

    struct Name {
        std::string value;

        template<Any Meta>
        static Name of() {
            return Name{std::string{detail::type_name<Meta>()}};
        }

        bool operator==(const Name&) const = default;

    private:
        explicit Name(std::string value) : value(std::move(value)) {}
    };
}