#pragma once

#include <iQSM/delta.h>
#include <iQSM/meta/aspect_id.h>
#include <iQSM/meta/concepts.h>

namespace iqsm::internals::delta {

    template<meta::Aspect Meta>
    inline auto make_atomic(Id<Meta> id, std::optional<Item<Meta>> before, std::optional<Item<Meta>> after) -> Delta
    {
        using Operation = typename iqsm::delta::FieldDiff<Meta>::Operation;

        auto field_delta = base::make_shared<iqsm::delta::FieldDiff<Meta>>();
        field_delta->ops.emplace(std::move(id), Operation{ std::move(before), std::move(after) });

        auto world_delta = base::make_shared<iqsm::delta::Fields>();
        world_delta->fields.emplace(types::aspectId<Meta>(), freeze(field_delta));
        return freeze(std::move(world_delta));
    }


    // Build a field-delta between two typed fields using pointer identity only:
    // - Items are "equal" iff the shared_ref pointer is equal.
    // - Any pointer change is treated as change (no structural equality).
    template<meta::Aspect Meta>
    inline auto count_delta_field(iqsm::cref<iqsm::FieldAbstract> from_untyped, iqsm::cref<iqsm::FieldAbstract> to_untyped)
        -> std::optional<iqsm::delta::UField>
    {
        const auto from = base::shared_ref_cast<const iqsm::FieldData<Meta>>(from_untyped);
        const auto to = base::shared_ref_cast<const iqsm::FieldData<Meta>>(to_untyped);

        using Operation = typename iqsm::delta::FieldDiff<Meta>::Operation;
        auto field_delta = base::make_shared<iqsm::delta::FieldDiff<Meta>>();

        // remove / change (iterate "from")
        for (const auto& kv : from->container) {
            const auto& id = kv.first;
            const auto& from_item = kv.second;

            if (not to->container.contains(id)) {
                field_delta->ops.emplace(id, Operation{ from_item, std::nullopt });
                continue;
            }

            const auto to_item = to->container.at(id);
            if (to_item != from_item) {
                field_delta->ops.emplace(id, Operation{ from_item, to_item });
            }
        }

        // add (iterate "to")
        for (const auto& kv : to->container) {
            const auto& id = kv.first;
            const auto& to_item = kv.second;
            if (from->container.contains(id)) continue;

            field_delta->ops.emplace(id, Operation{ std::nullopt, to_item });
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
