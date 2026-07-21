#pragma once

// Cereal-like describe: one visitor call per slot.
// Scope: one (Quantum) / all (Global). Shape: field / collection.

#include <string_view>

namespace fqsm::aspect {

    namespace detail::retrospection {

        template<auto Member, auto... Rest>
        decltype(auto) project(auto& root) {
            if constexpr (sizeof...(Rest) == 0) {
                return root.*Member;
            } else {
                return project<Rest...>(root.*Member);
            }
        }

        struct Sink {
            void aspect(std::string_view) {}
            void one(auto&&) {}
            void all(auto&&) {}
        };

    }

    template<auto... Members>
    struct Field {
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
    struct Collection {
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

    template<auto... Members>
    constexpr auto collection(std::string_view name) -> Collection<Members...> {
        return Collection<Members...>{.name = name};
    }

    template<typename Meta>
    concept HasRetrospection = requires(detail::retrospection::Sink& sink) {
        Meta::describe(sink);
    };

}
