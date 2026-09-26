#pragma once

#include <concepts>
#include <cstdint>

#include <base/serialization.h>
#include <fQSM/meta/retrospection.h>

namespace fqsm::detail::meta::category {

    struct RetrospectionProbe {
        void aspect(auto) {}
        void one(auto&&) {}
        void all(auto&&) {}
    };

}

namespace fqsm::aspect {

    // The five Q1 aspect categories. The erased runtime uses the same enum (erased::Category).
    enum class Category : std::uint8_t { entity, attribute, feature, component, group };

    // Category data of an aspect: every aspect base exposes it as Meta::Traits.
    template<Category C, typename Host = void, typename Element = void>
    struct Traits {
        static constexpr Category category = C;
        using HostAspect = Host;
        using ElementAspect = Element;
    };
}

namespace fqsm::meta::category {

    namespace musthave {
        template<typename T>
        concept Serialization = base::serialization::serializable<T>;

        template<typename Meta>
        concept Id = requires(const typename Meta::Id& id) {
            typename Meta::Id;
            { id.generate_random() } -> std::same_as<typename Meta::Id>;
        };

        template<typename Meta>
        concept Quantum = requires { typename Meta::Quantum; };

        template<typename Meta>
        concept Traits = requires { { Meta::Traits::category } -> std::convertible_to<aspect::Category>; };

        template<typename Meta>
        concept Primary = requires { typename Meta::PrimaryAspect; };

        template<typename Meta>
        concept Retrospection = requires(fqsm::detail::meta::category::RetrospectionProbe& d) {
            fqsm::aspect::Retrospection<Meta>::describe(d);
        };
    }

    // Any aspect: category data, an id and a quantum.
    template<typename Meta>
    concept Any = musthave::Traits<Meta> and musthave::Id<Meta> and musthave::Quantum<Meta>;

    template<typename Meta, aspect::Category C>
    concept Is = Any<Meta> and Meta::Traits::category == C;

    // Standalone aspects own their id; parasitic aspects share the id of their host.
    template<typename Meta>
    concept Standalone = Is<Meta, aspect::Category::entity>;

    template<typename Meta>
    concept Parasitic = Any<Meta> and Meta::Traits::category != aspect::Category::entity;

    template<typename Meta> concept Entity = Is<Meta, aspect::Category::entity>;
    template<typename Meta> concept Attribute = Is<Meta, aspect::Category::attribute>;
    template<typename Meta> concept Feature = Is<Meta, aspect::Category::feature>;
    template<typename Meta> concept Component = Is<Meta, aspect::Category::component>;
    template<typename Meta> concept Group = Is<Meta, aspect::Category::group>;

    template<typename Meta>
    concept Manipulation = musthave::Primary<Meta> and musthave::Id<Meta> and musthave::Quantum<Meta>;

}
