#pragma once

#include <concepts>

#include <base/serialization.h>

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
            //and base::serialization::serializable<typename Meta::Quantum>;

        template<typename Meta>
        concept Worker = requires { typename Meta::WorkerAspect; };

        template<typename Meta>
        concept Host = std::same_as<typename Meta::Id, typename Meta::HostAspect::Id>;

        template<typename Meta>
        concept Passport = requires(typename Meta::Quantum quantum) { quantum.passport; };

        template<typename Meta>
        concept Autonomy = not musthave::Host<Meta>;
    }

    // abstractions: can not be final concepts to be usef for domain archetypes:
    // (intended to be used in library code)
    template<typename Meta>
    concept Any = musthave::Id<Meta> and musthave::Quantum<Meta>;

    template<typename Meta>
    concept Standalone = Any<Meta> and musthave::Autonomy<Meta>;

    template<typename Meta>
    concept Parasitic = Any<Meta> and musthave::Host<Meta>;

    // final archetype kind to be used in domains aspect meta-classes
    template<typename Meta>
    concept Entity = Standalone<Meta>; // TODO: add Entity-specific requirements later...

    template<typename Meta>
    concept Component = Parasitic<Meta>;

    template<typename Meta>
    concept Attribute = Parasitic<Meta>; // TODO: add attribute-specific stuff, for example "rule of creation(ctor)"

    template<typename Meta>
    concept Group = Parasitic<Meta> and musthave::Worker<Meta>; // redesigned: "and musthave::Passport<Meta>"

}