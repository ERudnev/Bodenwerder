#pragma once

#include <functional>

#include <iQSM/world.h>
#include <iQSM/delta.h>
#include <iQSM/types.h>

namespace iqsm::validators::structural {
    template<Facet Anchor, Facet Dependee>
    using AnchorExtractFunc = std::function<typename Aspect<Anchor>::ItemId(typename Aspect<Dependee>::Item)>;

    template<Facet Anchor, Facet Dependee>
    Delta anchor(World, AnchorExtractFunc<Anchor, Dependee>);

    template<Facet Anchor, Facet Dependee>
    Delta anchor_quark(World);

    namespace detail {
        template<Facet Anchor, Facet Dependee, typename Extract>
        Delta anchor_impl(World, Extract);
    }
}

template<iqsm::Facet Anchor, iqsm::Facet Dependee, typename Extract>
iqsm::Delta iqsm::validators::structural::detail::anchor_impl(iqsm::World world, Extract extract)
{
    required(world, "anchor(): world");

    const auto anchor_field = world->field<Anchor>();
    const auto dependee_field = world->field<Dependee>();

    auto fd = std::make_shared<iqsm::delta::FieldState<Dependee>>();

    for (const auto& kv : dependee_field->container) {
        const auto& dependee_id = kv.first;
        const auto& dependee_item = kv.second;

        const auto anchor_id = extract(dependee_id, dependee_item);
        if (not anchor_field->container.contains(anchor_id)) {
            fd->deleted = fd->deleted.insert(dependee_id);
        }
    }

    if (fd->deleted.empty()) { return nullptr; }

    auto out = std::make_shared<iqsm::delta::WorldState>();
    out->fields = out->fields.insert(
        iqsm::Aspect<Dependee>::typeId,
        std::static_pointer_cast<const iqsm::delta::FieldUntyped>(iqsm::freeze(fd)));

    return iqsm::freeze(out);
}

template<iqsm::Facet Anchor, iqsm::Facet Dependee>
iqsm::Delta iqsm::validators::structural::anchor(iqsm::World world, AnchorExtractFunc<Anchor, Dependee> extract)
{
    return detail::anchor_impl<Anchor, Dependee>(world, [extract](auto, auto dependee_item) { return extract(dependee_item); });
}

template<iqsm::Facet Anchor, iqsm::Facet Dependee>
iqsm::Delta iqsm::validators::structural::anchor_quark(iqsm::World world)
{
    return detail::anchor_impl<Anchor, Dependee>(world, [](auto dependee_id, auto) { return dependee_id; });
}


