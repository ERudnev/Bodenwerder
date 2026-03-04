#pragma once

#include <initializer_list>
#include <type_traits>
#include <vector>

#include <iQSM/particles.h>
#include <iQSM/types.h>
#include <iQSM/internals/type_list.h>

namespace iqsm {
    template<typename Meta>
    concept Aspect = requires(const typename Meta::Id& id, const typename Meta::Quantum& val)
    {
        {id.generate_random()} -> std::same_as<typename Meta::Id>;
    };

    template<typename Meta>
    concept AspectXion = Aspect<Meta> && std::is_base_of_v<particles::Xion<Meta>, Meta>;

    template<typename Meta>
    concept AspectResource = Aspect<Meta> && std::is_base_of_v<particles::Resource<Meta>, Meta> && requires { typename Meta::Passport; };

    template<typename Meta>
    concept AspectParticle = Aspect<Meta> && (!AspectResource<Meta>);

    template<Aspect Meta>
    struct Facet : internals::Types {
        using Id = typename Meta::Id;
        using Quantum = typename Meta::Quantum;
        using Item = cref<Quantum>;
        // add Diff
        using TypeId = internals::Types::RuntimeId;
        inline static const TypeId typeId{typeid(Meta)};
        inline static const std::string_view typeName{internals::type_name(std::type_identity<Meta>{})};

        static Item create(std::initializer_list<int>) = delete; // forbids Facet<Meta>::create({})
        static Item create(Quantum value) { return std::make_shared<const Quantum>(static_cast<Quantum&&>(value)); }
    };

    template<typename... Deps>
    struct Require {
        using TypeId = internals::Types::RuntimeId;
        using Depends = internals::type_list<Deps...>;
        static std::vector<TypeId> depends() { return { TypeId(typeid(Deps))... }; }
    };
}


