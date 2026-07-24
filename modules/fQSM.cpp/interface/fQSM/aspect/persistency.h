#pragma once

// Cereal-like describe: one visitor call per slot.
// Scope: one (Quantum) / all (Global). Shape: field / collection.
// collection<Elem>() with no member pointers: Quantum (or Global) itself is the container (Group).
// Elem is mandatory — names the leaf type of each element (archivist layout / codecs).

#include <string_view>

namespace fqsm::aspect {

    namespace detail::retrospection {

        template<auto Member, auto... Rest>
        decltype(auto) project_member(auto& root) {
            if constexpr (sizeof...(Rest) == 0) {
                return root.*Member;
            } else {
                return project_member<Rest...>(root.*Member);
            }
        }

        template<auto... Members>
        decltype(auto) project(auto& root) {
            if constexpr (sizeof...(Members) == 0) {
                return root;
            } else {
                return project_member<Members...>(root);
            }
        }

    }

    template<auto... Members>
    struct Field {
        static_assert(sizeof...(Members) >= 1, "field<> requires at least one member pointer");

        std::string_view name{};

        template<typename Root>
        decltype(auto) get(Root& root) const {
            return detail::retrospection::project<Members...>(root);
        }

        template<typename Root>
        decltype(auto) get(const Root& root) const {
            return detail::retrospection::project<Members...>(root);
        }
    };

    template<typename Elem, auto... Members>
    struct Collection {
        using Element = Elem;

        std::string_view name{};

        template<typename Root>
        decltype(auto) get(Root& root) const {
            return detail::retrospection::project<Members...>(root);
        }

        template<typename Root>
        decltype(auto) get(const Root& root) const {
            return detail::retrospection::project<Members...>(root);
        }
    };

    template<auto... Members>
    constexpr auto field(std::string_view name) -> Field<Members...> {
        return Field<Members...>{.name = name};
    }

    template<typename Elem, auto... Members>
    constexpr auto collection(std::string_view name) -> Collection<Elem, Members...> {
        return Collection<Elem, Members...>{.name = name};
    }

}
