#pragma once

#include <type_traits>
#include <utility>
#include <vector>
#include <memory>

#include <iQSM/world.h>
#include <iQSM/delta.h>
#include <iQSM/operations/integration.h>
#include <iQSM/types.h>



namespace iqsm::ops::validation {

    struct List {
        std::vector<Func> list;
        // Semantics of a validator: (World) -> Delta
        Delta apply(World world) const;
    };

    struct Structural {
        template<meta::Aspect Anchor, meta::Aspect Dependee, auto Member>
        static Delta anchor(World);

        template<meta::Aspect Anchor, meta::Aspect Dependee>
        static Delta anchor_attribute(World); // confinement

        // Ensures Dependee existence from Anchor by predicate.
        template<meta::Aspect Dependee, meta::Aspect Anchor, auto Need>
        static Delta existence(World);

        template<meta::Aspect Anchor, meta::Aspect Dependee, auto Construct>
        static Delta anchor_component(World); // 1-1 iff confinement

        template<meta::Aspect Anchor, meta::Aspect Dependee, auto Member>
        static Delta anchor_any(World);

        template<meta::Aspect Anchor, meta::Aspect Dependee, auto Member>
        static Delta anchor_all(World);
    };
} // namespace iqsm::ops::validation

namespace iqsm::detail::ops::validation {
    template<meta::Aspect Anchor, meta::Aspect Dependee, typename Extract>
    Delta anchor_impl(World world, Extract extract)
    {
        const auto anchor_field = world->field<Anchor>();
        const auto dependee_field = world->field<Dependee>();

        auto fd = base::make_shared<delta::FieldDiff<Dependee>>();

        for (const auto& kv : dependee_field->container) {
            const auto& dependee_id = kv.first;
            const auto& dependee_item = kv.second;

            const auto anchor_id = extract(dependee_id, dependee_item);
            if (not anchor_field->container.contains(anchor_id)) {
                auto op = typename delta::FieldDiff<Dependee>::Operation{};
                op.remove = true;
                fd->ops = fd->ops.insert(dependee_id, std::move(op));
            }
        }

        if (fd->ops.empty()) { return ::iqsm::delta::empty(); }

        auto out = base::make_shared<delta::Fields>();
        out->fields = out->fields.insert(
            Facet<Dependee>::typeId,
            freeze(fd));

        return freeze(out);
    }
} // namespace iqsm::detail::ops::validation

namespace iqsm::ops::validation {
    template<meta::Aspect Anchor, meta::Aspect Dependee, auto Member>
    Delta Structural::anchor(World world)
    {
        static_assert(std::is_member_object_pointer_v<decltype(Member)>);

        using Quantum = typename Facet<Dependee>::Quantum;
        using AnchorId = typename Facet<Anchor>::Id;
        using MemberValue = decltype(std::declval<Quantum>().*Member);
        static_assert(std::is_convertible_v<MemberValue, AnchorId>);

        return detail::ops::validation::anchor_impl<Anchor, Dependee>(world, [](auto, auto dependee_item) -> AnchorId {
            return static_cast<AnchorId>(dependee_item.get()->*Member);
        });
    }

    template<meta::Aspect Anchor, meta::Aspect Dependee>
    Delta Structural::anchor_attribute(World world)
    {
        return detail::ops::validation::anchor_impl<Anchor, Dependee>(world, [](auto dependee_id, auto) { return dependee_id; });
    }

    template<meta::Aspect Dependee, meta::Aspect Anchor, auto Need>
    Delta Structural::existence(World world)
    {
        using AnchorQuantum = typename Facet<Anchor>::Quantum;
        using DependeeQuantum = typename Facet<Dependee>::Quantum;

        static_assert(std::is_invocable_r_v<bool, decltype(Need), World, const AnchorQuantum&>);

        const auto anchor_field = world->field<Anchor>();
        const auto dependee_field = world->field<Dependee>();

        auto fd = base::make_shared<delta::FieldDiff<Dependee>>();

        for (const auto& kv : anchor_field->container) {
            const auto& id = kv.first;
            const auto& anchor_item = kv.second;
            const bool need = Need(world, *anchor_item);

            const bool has = dependee_field->container.contains(id);

            if (need) {
                if (has) continue;
                typename delta::FieldDiff<Dependee>::Operation op{};
                op.add = Facet<Dependee>::create(DependeeQuantum{});
                fd->ops = fd->ops.insert(id, std::move(op));
            } else {
                if (not has) continue;
                typename delta::FieldDiff<Dependee>::Operation op{};
                op.remove = true;
                fd->ops = fd->ops.insert(id, std::move(op));
            }
        }

        if (fd->ops.empty()) { return ::iqsm::delta::empty(); }

        auto out = base::make_shared<delta::Fields>();
        out->fields = out->fields.insert(
            Facet<Dependee>::typeId,
            freeze(fd));

        return freeze(out);
    }

    template<meta::Aspect Anchor, meta::Aspect Dependee, auto Construct>
    Delta Structural::anchor_component(World world)
    {
        using AnchorId = typename Facet<Anchor>::Id;
        using DependeeId = typename Facet<Dependee>::Id;
        using AnchorQuantum = typename Facet<Anchor>::Quantum;
        using DependeeQuantum = typename Facet<Dependee>::Quantum;
        static_assert(std::is_same_v<AnchorId, DependeeId>);
        static_assert(std::is_invocable_r_v<DependeeQuantum, decltype(Construct), World, const AnchorQuantum&>);

        const auto anchor_field = world->field<Anchor>();
        const auto dependee_field = world->field<Dependee>();

        auto fd = base::make_shared<delta::FieldDiff<Dependee>>();

        // Necessary: Dependee cannot outlive Anchor.
        for (const auto& kv : dependee_field->container) {
            const auto& dependee_id = kv.first;
            if (not anchor_field->container.contains(dependee_id)) {
                auto op = typename delta::FieldDiff<Dependee>::Operation{};
                op.remove = true;
                fd->ops = fd->ops.insert(dependee_id, std::move(op));
            }
        }

        // Sufficient: Anchor implies Dependee.
        for (const auto& kv : anchor_field->container) {
            const auto& anchor_id = kv.first;
            const auto& anchor_item = kv.second;
            if (not dependee_field->container.contains(anchor_id)) {
                auto op = typename delta::FieldDiff<Dependee>::Operation{};
                op.add = Facet<Dependee>::create(Construct(world, *anchor_item));
                fd->ops = fd->ops.insert(anchor_id, std::move(op));
            }
        }

        if (fd->ops.empty()) { return ::iqsm::delta::empty(); }

        auto out = base::make_shared<delta::Fields>();
        out->fields = out->fields.insert(
            Facet<Dependee>::typeId,
            freeze(fd));

        return freeze(out);
    }

    template<meta::Aspect Anchor, meta::Aspect Dependee, auto Member>
    // anchor(any):
    // - Dependee holds std::vector<Anchor::Id>
    // - prunes missing anchor ids from that vector
    // - deletes dependee only if the vector becomes empty
    Delta Structural::anchor_any(World world)
    {
        static_assert(std::is_member_object_pointer_v<decltype(Member)>);

        using Quantum = typename Facet<Dependee>::Quantum;
        using AnchorId = typename Facet<Anchor>::Id;
        using MemberValue = std::remove_cvref_t<decltype(std::declval<Quantum>().*Member)>;
        static_assert(std::is_same_v<MemberValue, std::vector<AnchorId>>);

        const auto anchor_field = world->field<Anchor>();
        const auto dependee_field = world->field<Dependee>();

        auto fd = base::make_shared<delta::FieldDiff<Dependee>>();

        for (const auto& kv : dependee_field->container) {
            const auto& dependee_id = kv.first;
            const auto& dependee_item = kv.second;

            const auto& ids = (dependee_item.get()->*Member);

            std::vector<AnchorId> filtered;
            filtered.reserve(ids.size());
            for (const auto& id : ids) {
                if (anchor_field->container.contains(id)) {
                    filtered.push_back(id);
                }
            }

            if (filtered.empty()) {
                auto op = typename delta::FieldDiff<Dependee>::Operation{};
                op.remove = true;
                fd->ops = fd->ops.insert(dependee_id, std::move(op));
                continue;
            }

            if (filtered.size() != ids.size()) {
                Quantum q = *dependee_item;
                q.*Member = std::move(filtered);
                auto op = typename delta::FieldDiff<Dependee>::Operation{};
                op.change = std::pair<typename delta::FieldDiff<Dependee>::Item, typename delta::FieldDiff<Dependee>::Item>{
                    dependee_item,
                    Facet<Dependee>::create(std::move(q)),
                };
                fd->ops = fd->ops.insert(dependee_id, std::move(op));
            }
        }

        if (fd->ops.empty()) { return ::iqsm::delta::empty(); }

        auto out = base::make_shared<delta::Fields>();
        out->fields = out->fields.insert(
            Facet<Dependee>::typeId,
            freeze(fd));

        return freeze(out);
    }

    template<meta::Aspect Anchor, meta::Aspect Dependee, auto Member>
    // anchor(all):
    // - Dependee holds std::vector<Anchor::Id>
    // - deletes dependee if any anchor id is missing
    Delta Structural::anchor_all(World world)
    {
        static_assert(std::is_member_object_pointer_v<decltype(Member)>);

        using Quantum = typename Facet<Dependee>::Quantum;
        using AnchorId = typename Facet<Anchor>::Id;
        using MemberValue = std::remove_cvref_t<decltype(std::declval<Quantum>().*Member)>;
        static_assert(std::is_same_v<MemberValue, std::vector<AnchorId>>);

        const auto anchor_field = world->field<Anchor>();
        const auto dependee_field = world->field<Dependee>();

        auto fd = base::make_shared<delta::FieldDiff<Dependee>>();

        for (const auto& kv : dependee_field->container) {
            const auto& dependee_id = kv.first;
            const auto& dependee_item = kv.second;

            const auto& ids = (dependee_item.get()->*Member);

            bool ok = true;
            for (const auto& id : ids) {
                if (not anchor_field->container.contains(id)) {
                    ok = false;
                    break;
                }
            }

            if (not ok) {
                auto op = typename delta::FieldDiff<Dependee>::Operation{};
                op.remove = true;
                fd->ops = fd->ops.insert(dependee_id, std::move(op));
            }
        }

        if (fd->ops.empty()) { return ::iqsm::delta::empty(); }

        auto out = base::make_shared<delta::Fields>();
        out->fields = out->fields.insert(
            Facet<Dependee>::typeId,
            freeze(fd));

        return freeze(out);
    }
} // namespace iqsm::ops::validation
