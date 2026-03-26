#pragma once

#include <concepts>
#include <type_traits>

namespace iqsm::meta {
    template<typename Meta>
    concept HasId = requires { typename Meta::Id; };

    template<typename Meta>
    concept HasQuantum = requires { typename Meta::Quantum; };

    template<typename Meta>
    concept HasParentAspect = requires { typename Meta::ParentAspect; };

    template<typename Meta>
    concept HasPassport = requires { typename Meta::Passport; };

    template<typename Meta>
    concept Aspect =
        HasId<Meta> && HasQuantum<Meta> &&
        requires(const typename Meta::Id& id, const typename Meta::Quantum& /*val*/) {
            { id.generate_random() } -> std::same_as<typename Meta::Id>;
        };

    template<typename Meta>
    concept Entity = Aspect<Meta> && (not HasParentAspect<Meta>) && (not HasPassport<Meta>);

    template<typename Meta>
    concept Resource =
        Aspect<Meta> &&
        HasPassport<Meta> &&
        (not HasParentAspect<Meta>);

    template<typename Meta>
    concept Quark = Aspect<Meta> && HasParentAspect<Meta> && (not HasPassport<Meta>);

    template<typename Meta>
    concept Attribute = Quark<Meta>;

    template<typename Meta>
    concept Component = Quark<Meta>;

    template<typename Meta>
    concept Particle = Entity<Meta> || Quark<Meta>;
}

