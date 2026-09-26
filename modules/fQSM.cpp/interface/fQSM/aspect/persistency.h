#pragma once

// Cereal-like describe via Retrospection<T> specialization (not a member of T).
// Scope: one (instance / Quantum) / all (Global). Shape: field / collection.
// collection<Elem>() with no member pointers: root itself is the container (Group).
// Elem is mandatory — names the element type (atom or nested Retrospection).

#include <string_view>

#include <fQSM/meta/retrospection.h>

namespace fqsm::aspect {

    namespace detail::retrospection {
        // root.*m1.*m2...; no members: root itself
        template<auto... Members>
        decltype(auto) project(auto& root) { return (root .* ... .* Members); }
    }

    template<auto... Members>
    struct Field {
        static_assert(sizeof...(Members) >= 1, "field<> requires at least one member pointer");

        std::string_view name{};

        // Root may be const: the projection keeps it
        template<typename Root>
        decltype(auto) get(Root& root) const {
            return detail::retrospection::project<Members...>(root);
        }
    };

    template<typename Elem, auto... Members>
    struct Collection {
        using Element = Elem;

        std::string_view name{};
        // Column base name for map keys (umap / pair<Key, Mapped> elements). Ignored for sequences.
        std::string_view key_name = "key";

        // Root may be const: the projection keeps it
        template<typename Root>
        decltype(auto) get(Root& root) const {
            return detail::retrospection::project<Members...>(root);
        }
    };

    template<auto... Members>
    constexpr auto field(std::string_view name) -> Field<Members...> {
        return Field<Members...>{.name = name};
    }

    template<typename Elem, auto... Members>
    constexpr auto collection(std::string_view name, std::string_view key_name = "key")
        -> Collection<Elem, Members...> {
        return Collection<Elem, Members...>{.name = name, .key_name = key_name};
    }

}
