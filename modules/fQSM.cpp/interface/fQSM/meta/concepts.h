#pragma once

#include <concepts>

namespace fqsm::meta::aspect {
    
    namespace has {
        template<typename Meta>
        concept Id = requires(const typename Meta::Id& id) {
            typename Meta::Id;
            { id.generate_random() } -> std::same_as<typename Meta::Id>;
        };
    
        template<typename Meta>
        concept Quantum = requires { typename Meta::Quantum; };

        template<typename Meta>
        concept Worker = requires { typename Meta::WorkerAspect; };

        template<typename Meta>
        concept Host = std::same_as<typename Meta::Id, typename Meta::HostAspect::Id>;
    
        template<typename Meta>
        concept Passport = requires(typename Meta::Quantum quantum) { quantum.passport; };

        template<typename Meta>
        concept Autonomy = not has::Host<Meta>;
    }

    // abstractions: can not be final concepts to be usef for domain archetypes:
    // (intended to be used in library code)
    template<typename Meta>
    concept Any = has::Id<Meta> and has::Quantum<Meta>;

    template<typename Meta>
    concept Standalone = Any<Meta> and has::Autonomy<Meta>;

    template<typename Meta>
    concept Parasitic = Any<Meta> and has::Host<Meta>;

    // final archetype kind to be used in domains aspect meta-classes
    template<typename Meta>
    concept Entity = Standalone<Meta>; // TODO: add Entity-specific requirements later...

    template<typename Meta>
    concept Controller = Standalone<Meta> and has::Passport<Meta> and has::Worker<Meta>;

    template<typename Meta>
    concept Component = Parasitic<Meta>;

    template<typename Meta>
    concept Attribute = Parasitic<Meta>; // TODO: add attribute-specific stuff, for example "rule of creation(ctor)"

}