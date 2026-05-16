#pragma once

// KQM: &iQSM::Aspect

#include <type_traits>

#include <fQSM/meta/concepts/archetypes.h>

namespace fqsm::meta::aspect {
    namespace has {
        template<typename Meta>
        concept Runtime = requires {
            typename Meta::Runtime;
        };
        
    }
    template<typename Meta>
    concept Any = has::Runtime<Meta> and archetype::Any<Meta>;

    template<typename Meta>
    concept Host = has::Runtime<Meta> and archetype::Host<Meta>;

    template<typename Meta>
    concept Parasite = has::Runtime<Meta> and archetype::Parasite<Meta>;

    template<typename Meta>
    concept Entity = has::Runtime<Meta> and archetype::Entity<Meta>;

    template<typename Meta>
    concept Controller = has::Runtime<Meta> and archetype::Controller<Meta>;

    template<typename Meta>
    concept Component = has::Runtime<Meta> and archetype::Component<Meta>;

    template<typename Meta>
    concept Attribute = has::Runtime<Meta> and archetype::Attribute<Meta>;
}
