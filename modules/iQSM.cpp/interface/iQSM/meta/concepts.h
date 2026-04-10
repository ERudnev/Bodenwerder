#pragma once

#include <concepts>
#include <type_traits>

namespace iqsm::meta {
    template<typename Meta>
    concept HasId = requires { typename Meta::Id; };

    template<typename Meta>
    concept HasQuantum = requires { typename Meta::Quantum; };

    template<typename Meta>
    concept HasGlobal = requires { typename Meta::Global; };

    template<typename Meta>
    concept HasParentAspect = requires { typename Meta::ParentAspect; };

    template<typename Meta>
    concept HasQuantumPassport = requires(typename Meta::Quantum quantum) { quantum.passport; };

    template<typename Meta>
    concept Aspect =
        HasId<Meta> && HasQuantum<Meta> && HasGlobal<Meta> &&
        requires(const typename Meta::Id& id, const typename Meta::Quantum& /*val*/) {
            { id.generate_random() } -> std::same_as<typename Meta::Id>;
        };


    // TODO fix this semantic debt: are Entity ad Handle siblings or not?
    template<typename Meta>
    concept Entity = Aspect<Meta> && (not HasParentAspect<Meta>) && (not HasQuantumPassport<Meta>);

    // Handle = world-side declaration of an external runtime object.
    // - Quantum lives in World and must carry `passport` (+ optional domain usage state).
    // - Runtime object is managed by binding layer and may be type-erased.
    template<typename Meta>
    concept Handle = Aspect<Meta> && HasQuantumPassport<Meta> && (not HasParentAspect<Meta>);

    template<typename Meta>
    concept Quark = Aspect<Meta> && HasParentAspect<Meta>;

    template<typename Meta>
    concept Attribute = Quark<Meta>;

    template<typename Meta>
    concept Component = Quark<Meta>;

    template<typename Meta>
    concept Particle = Entity<Meta> || Quark<Meta> || Handle<Meta>;
}

