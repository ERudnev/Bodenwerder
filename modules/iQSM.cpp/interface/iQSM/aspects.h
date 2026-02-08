#pragma once

#include <initializer_list>
#include <vector>

#include <iQSM/types.h>

namespace iqsm {
    template<typename Meta>
    concept Facet = requires(const typename Meta::Id& id, const typename Meta::ItemState& val)
    {
        {id.generate_random()} -> std::same_as<typename Meta::Id>;
    };

    template<Facet Meta>
    struct Aspect : internals::Types {
        using ItemId = typename Meta::Id;
        using ItemState = typename Meta::ItemState;
        using Item = cref<ItemState>;
        using TypeId = internals::Types::RuntimeId;
        inline static const TypeId typeId{typeid(Meta)};
        inline static const std::string_view typeName{internals::type_name(std::type_identity<Meta>{})};

        static Item create(std::initializer_list<int>) = delete; // forbids Aspect<Meta>::create({})
        static Item create(ItemState state) { return std::make_shared<const ItemState>(static_cast<ItemState&&>(state)); }
    };

    template<typename... Deps>
    struct DependsFrom {
        using TypeId = internals::Types::RuntimeId;
        static std::vector<TypeId> depends() { return { TypeId(typeid(Deps))... }; }
    };
}


