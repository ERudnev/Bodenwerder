#pragma once

#include <type_traits>
#include <utility>
#include <vector>

#include <iQSM/world.h>
#include <iQSM/delta.h>
#include <iQSM/operations/integration.h>
#include <iQSM/types.h>



namespace iqsm::ops::validation {

    using Func = Delta(*)(World);

    struct List {
        std::vector<Func> list;
        // Semantics of a validator: (World) -> Delta
        Delta apply(World world) const;
    };

    struct Structural {
        template<Facet Anchor, Facet Dependee, auto Member>
        static Delta anchor(World);

        template<Facet Anchor, Facet Dependee>
        static Delta anchor_quark(World); // confinement
    };
    namespace detail {
        template<Facet Anchor, Facet Dependee, typename Extract>
        Delta anchor_impl(World, Extract);
    }
}

namespace iqsm::ops::validation::detail {
    template<Facet Anchor, Facet Dependee, typename Extract>
    Delta anchor_impl(World world, Extract extract)
    {
        required(world, "anchor(): world");

        const auto anchor_field = world->field<Anchor>();
        const auto dependee_field = world->field<Dependee>();

        auto fd = std::make_shared<delta::FieldState<Dependee>>();

        for (const auto& kv : dependee_field->container) {
            const auto& dependee_id = kv.first;
            const auto& dependee_item = kv.second;

            const auto anchor_id = extract(dependee_id, dependee_item);
            if (not anchor_field->container.contains(anchor_id)) {
                fd->deleted = fd->deleted.insert(dependee_id);
            }
        }

        if (fd->deleted.empty()) { return nullptr; }

        auto out = std::make_shared<delta::WorldState>();
        out->fields = out->fields.insert(
            Aspect<Dependee>::typeId,
            std::static_pointer_cast<const delta::FieldUntyped>(freeze(fd)));

        return freeze(out);
    }
}

namespace iqsm::ops::validation {
    template<Facet Anchor, Facet Dependee, auto Member>
    Delta Structural::anchor(World world)
    {
        static_assert(std::is_member_object_pointer_v<decltype(Member)>);

        using Quantum = typename Aspect<Dependee>::Quantum;
        using AnchorId = typename Aspect<Anchor>::ItemId;
        using MemberValue = decltype(std::declval<Quantum>().*Member);
        static_assert(std::is_convertible_v<MemberValue, AnchorId>);

        return detail::anchor_impl<Anchor, Dependee>(world, [](auto, auto dependee_item) -> AnchorId {
            required(dependee_item, "anchor(): dependee item");
            return static_cast<AnchorId>(dependee_item.get()->*Member);
        });
    }

    template<Facet Anchor, Facet Dependee>
    Delta Structural::anchor_quark(World world)
    {
        return detail::anchor_impl<Anchor, Dependee>(world, [](auto dependee_id, auto) { return dependee_id; });
    }
}
