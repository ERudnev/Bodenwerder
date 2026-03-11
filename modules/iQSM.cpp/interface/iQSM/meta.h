#pragma once

#include <initializer_list>
#include <type_traits>
#include <vector>

#include <base/shared_reference.h>
#include <boost/pfr/ops_fields.hpp>
#include <iQSM/aspects.h>
#include <iQSM/types.h>
#include <iQSM/internals/type_list.h>

namespace iqsm::meta {

    template<typename Meta>
    concept Aspect = requires(const typename Meta::Id& id, const typename Meta::Quantum& val)
    {
        {id.generate_random()} -> std::same_as<typename Meta::Id>;
    };

    template<typename Meta>
    concept Entity = Aspect<Meta> && std::is_base_of_v<aspects::Entity<Meta>, Meta>;

    template<typename Meta>
    concept Resource = Aspect<Meta> && std::is_base_of_v<aspects::Resource<Meta>, Meta> && requires { typename Meta::Passport; };

    template<typename Meta>
    concept Attribute = Aspect<Meta>
        && requires { typename Meta::ParentAspect; }
        && std::is_base_of_v<aspects::Attribute<Meta, typename Meta::ParentAspect>, Meta>;

    template<typename Meta>
    concept Component = Aspect<Meta>
        && requires { typename Meta::ParentAspect; }
        && std::is_base_of_v<aspects::Component<Meta, typename Meta::ParentAspect>, Meta>;

    template<typename Meta>
    concept Quark = Attribute<Meta> || Component<Meta>;

    template<typename Meta>
    concept Particle = Entity<Meta> || Quark<Meta>;
}

namespace iqsm::detail::_meta {
    struct EmptyGlobal {};

    template<typename Meta, typename = void>
    struct global_of { using type = EmptyGlobal; };

    template<typename Meta>
    struct global_of<Meta, std::void_t<typename Meta::Global>> { using type = typename Meta::Global; };
}

namespace iqsm {

    template<meta::Aspect Meta>
    struct Facet : internals::Types {
        using Id = typename Meta::Id;
        using Quantum = typename Meta::Quantum;
        using Item = cref<Quantum>;
        using GlobalData = typename detail::_meta::global_of<Meta>::type;
        using Global = cref<GlobalData>;
        // add Diff
        using TypeId = internals::Types::RuntimeId;

        inline static const TypeId typeId{typeid(Meta)};
        inline static const std::string_view typeName{internals::type_name(std::type_identity<Meta>{})};

        static Item create(std::initializer_list<int>) = delete; // forbids Facet<Meta>::create({})
        static Item create(Quantum value) { return base::make_shared<const Quantum>(static_cast<Quantum&&>(value)); }

        static bool equal(const Quantum& a, const Quantum& b) { return boost::pfr::eq_fields(a, b); }
    };

    template<typename... Deps>
    struct Require {
        using TypeId = internals::Types::RuntimeId;
        using Depends = internals::type_list<Deps...>;
        static std::vector<TypeId> depends() { return { TypeId(typeid(Deps))... }; }
    };
}

