#pragma once

#include <concepts>
#include <cstddef>
#include <type_traits>
#include <utility>

#include <boost/pfr/core.hpp>

#include <fQSM/meta/pfr.h>

namespace fqsm::model::elementary {

    // Elementary state identity: one short-circuiting equality walk.
    //
    // Conserved building block for later noop elision (e.g. scrub patch vs reality on
    // Writing collapse). Not wired into QuantumGate — gates leave honest patchlets.
    //
    // Order matters and is intentional (do not reorder casually):
    // 1) associative map-like — never container operator== (sprawls/hard-errors on mapped_type);
    // 2) sequence-like (vector, string, …) — never container operator== (same for value_type);
    // 3) std::equality_comparable — type's own == when well-formed and not a container above;
    // 4) Boost.PFR aggregates — field-wise recursion via equal_value.
    //
    // Builtin/container policies stay localized here. Do not push operator== onto domain types.

    template<typename T>
    concept map_like = requires(const T& m, const typename T::key_type& key) {
        typename T::key_type;
        typename T::mapped_type;
        { m.size() } -> std::convertible_to<std::size_t>;
        { m.find(key) };
        { m.begin() };
        { m.end() };
    };

    template<typename T>
    concept sequence_like = requires(const T& s) {
        typename T::value_type;
        { s.size() } -> std::convertible_to<std::size_t>;
        { s.begin() };
        { s.end() };
    } && (!map_like<T>);

    template<typename T>
    constexpr bool is_equalable();

    template<typename T>
    constexpr bool is_equalable() {
        using bare = std::remove_cvref_t<T>;
        if constexpr (map_like<bare>) {
            return is_equalable<typename bare::mapped_type>();
        } else if constexpr (sequence_like<bare>) {
            return is_equalable<typename bare::value_type>();
        } else if constexpr (std::equality_comparable<bare>) {
            return true;
        } else if constexpr (::fqsm::detail::meta::pfr_structure<bare>) {
            return []<std::size_t... I>(std::index_sequence<I...>) {
                return (is_equalable<boost::pfr::tuple_element_t<I, bare>>() && ...);
            }(std::make_index_sequence<boost::pfr::tuple_size_v<bare>>{});
        } else {
            return false;
        }
    }

    template<typename T>
    constexpr bool equal_value(const T& lhs, const T& rhs) {
        static_assert(is_equalable<T>(), "elementary::equal_value: type is not equalable");
        using bare = std::remove_cvref_t<T>;
        if constexpr (map_like<bare>) {
            if (lhs.size() != rhs.size()) {
                return false;
            }
            for (const auto& [key, value] : lhs) {
                const auto found = rhs.find(key);
                if (found == rhs.end()) {
                    return false;
                }
                if (not equal_value(value, found->second)) {
                    return false;
                }
            }
            return true;
        } else if constexpr (sequence_like<bare>) {
            if (lhs.size() != rhs.size()) {
                return false;
            }
            auto left = lhs.begin();
            auto right = rhs.begin();
            while (left != lhs.end()) {
                if (right == rhs.end()) {
                    return false;
                }
                if (not equal_value(*left, *right)) {
                    return false;
                }
                ++left;
                ++right;
            }
            return right == rhs.end();
        } else if constexpr (std::equality_comparable<bare>) {
            return lhs == rhs;
        } else {
            return [&]<std::size_t... I>(std::index_sequence<I...>) {
                return (equal_value(boost::pfr::get<I>(lhs), boost::pfr::get<I>(rhs)) && ...);
            }(std::make_index_sequence<boost::pfr::tuple_size_v<bare>>{});
        }
    }

}
