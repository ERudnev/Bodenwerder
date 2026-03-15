#pragma once

#include <iQSM/delta.h>

namespace iqsm::internals::delta {

    template<meta::Aspect Meta>
    inline auto make_atomic(Id<Meta> id, std::optional<Item<Meta>> add, std::optional<std::pair<Item<Meta>, Item<Meta>>> change, bool remove) -> Delta
    {
        using Operation = typename iqsm::delta::FieldDiff<Meta>::Operation;

        auto field_delta = base::make_shared<iqsm::delta::FieldDiff<Meta>>();
        field_delta->ops = field_delta->ops.insert(std::move(id), Operation{ std::move(add), std::move(change), remove });

        auto world_delta = base::make_shared<iqsm::delta::Fields>();
        world_delta->fields = world_delta->fields.insert(Facet<Meta>::typeId, freeze(field_delta));
        return freeze(std::move(world_delta));
    }


    // Build a field-delta between two typed fields using pointer identity only:
    // - Items are "equal" iff the shared_ref pointer is equal.
    // - Any pointer change is treated as change (no structural equality).
    template<meta::Aspect Meta>
    inline auto count_delta_field(iqsm::FieldAbstract::Ref from_untyped, iqsm::FieldAbstract::Ref to_untyped)
        -> std::optional<iqsm::delta::UField>
    {
        const auto from = base::shared_ref_cast<const iqsm::FieldObject<Meta>>(from_untyped);
        const auto to = base::shared_ref_cast<const iqsm::FieldObject<Meta>>(to_untyped);

        using Operation = typename iqsm::delta::FieldDiff<Meta>::Operation;
        auto field_delta = base::make_shared<iqsm::delta::FieldDiff<Meta>>();

        // remove / change (iterate "from")
        for (const auto& kv : from->container) {
            const auto& id = kv.first;
            const auto& from_item = kv.second;

            if (not to->container.contains(id)) {
                field_delta->ops = field_delta->ops.insert(id, Operation{ std::nullopt, std::nullopt, true });
                continue;
            }

            const auto to_item = to->container.at(id);
            if (to_item != from_item) {
                field_delta->ops = field_delta->ops.insert(
                    id,
                    Operation{
                        std::nullopt,
                        std::pair<typename iqsm::delta::FieldDiff<Meta>::Item, typename iqsm::delta::FieldDiff<Meta>::Item>{ from_item, to_item },
                        false,
                    });
            }
        }

        // add (iterate "to")
        for (const auto& kv : to->container) {
            const auto& id = kv.first;
            const auto& to_item = kv.second;
            if (from->container.contains(id)) continue;

            field_delta->ops = field_delta->ops.insert(id, Operation{ to_item, std::nullopt, false });
        }

        // global
        if (from->global != to->global) {
            field_delta->global_change = std::pair<typename iqsm::delta::FieldDiff<Meta>::Global, typename iqsm::delta::FieldDiff<Meta>::Global>{
                from->global,
                to->global,
            };
        }

        if (field_delta->ops.empty() && not field_delta->global_change.has_value()) return std::nullopt;
        return iqsm::freeze(std::move(field_delta));
    }
}
