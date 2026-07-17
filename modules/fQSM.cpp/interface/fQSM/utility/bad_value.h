#pragma once

#include <cstddef>
#include <type_traits>
#include <utility>

#include <boost/pfr/core.hpp>

#include <fQSM/identifier.h>
#include <fQSM/meta/pfr.h>

namespace fqsm::utility {
namespace detail {

    template<typename T>
    constexpr bool bad_value_always_false = false;

    template<typename T>
    struct MakeBadValue {
        static T apply() {
            if constexpr (std::is_default_constructible_v<T>) {
                return T{};
            } else if constexpr (fqsm::detail::meta::pfr_structure<T>) {
                return []<std::size_t... I>(std::index_sequence<I...>) {
                    return T{ MakeBadValue<boost::pfr::tuple_element_t<I, T>>::apply()... };
                }(std::make_index_sequence<boost::pfr::tuple_size_v<T>>{});
            } else {
                static_assert(bad_value_always_false<T>,
                    "fqsm::utility::BadValue: type is neither default-constructible nor a PFR aggregate");
            }
        }
    };

    template<typename Meta, typename BaseType>
    struct MakeBadValue<Identifier<Meta, BaseType>> {
        static Identifier<Meta, BaseType> apply() {
            return Identifier<Meta, BaseType>::bad();
        }
    };

} // namespace detail

    struct BadValue {
        template<typename T>
        operator T() const {
            return detail::MakeBadValue<T>::apply();
        }
    };

} // namespace fqsm::utility
