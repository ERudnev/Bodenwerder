#pragma once

#include <concepts>
#include <type_traits>

#include <boost/pfr/traits.hpp>

namespace fqsm::detail::meta {

    // fQSM tag for Boost.PFR reflectability queries.
    struct PfrFor {};

    template<typename T>
    using bare = std::remove_cvref_t<T>;

    // Valid PFR structure: aggregate that Boost.PFR can reflect field-by-field (e.g. Quantum).
    template<typename T>
    concept pfr_structure = boost::pfr::is_implicitly_reflectable_v<bare<T>, PfrFor>;
}
