#include <iQSM/repository/agents/collaboration.h>

#include <iQSM/helpers/schema.h>
#include <iQSM/operations/integration.h>
#include <iQSM/references.h>
#include <iQSM/resources/manager.h>

namespace {

    auto make_overlap_slice_mutable(iqsm::Schema overlap_schema) -> iqsm::ref<iqsm::WorldObject> {
        const iqsm::Schema empty_schema = iqsm::helpers::schema::assemble<>();
        auto manager = base::make_shared<iqsm::resources::ManagerCore>(empty_schema);
        return base::make_shared<iqsm::WorldObject>(std::move(overlap_schema), iqsm::freeze(manager));
    }

    auto project_overlap(
        iqsm::World world,
        iqsm::Schema overlapSchema,
        const iqsm::SchemaObject::TypeSet& overlapTypes) -> iqsm::World
    {
        auto projected = make_overlap_slice_mutable(overlapSchema);

        for (const auto& typeId : overlapTypes) {
            const auto& entry = overlapSchema->aspects.at(typeId);
            const auto field = world->field(typeId);

            if (field == entry.field.zero) {
                continue;
            }

            projected->fields = projected->fields.insert(typeId, field);
        }

        return iqsm::freeze(projected);
    }

    auto merge_overlap_field(
        const iqsm::SchemaObject::Entry& entry,
        base::pair<iqsm::cref<iqsm::FieldAbstract>> lastFields,
        base::pair<iqsm::cref<iqsm::FieldAbstract>> currentFields) -> iqsm::cref<iqsm::FieldAbstract>
    {
        if (currentFields.first == currentFields.second) {
            return currentFields.first;
        }

        const bool leftChanged = currentFields.first != lastFields.first;
        const bool rightChanged = currentFields.second != lastFields.second;

        if (leftChanged && !rightChanged) {
            return currentFields.first;
        }
        if (!leftChanged && rightChanged) {
            return currentFields.second;
        }

        auto merged = leftChanged || rightChanged ? lastFields.first : entry.field.zero;

        if (const auto leftDelta = entry.delta.make_delta_field(
            leftChanged ? lastFields.first : entry.field.zero,
            currentFields.first); leftDelta.has_value())
        {
            merged = entry.delta.integrate_field(merged, *leftDelta);
        }

        if (const auto rightDelta = entry.delta.make_delta_field(
            rightChanged ? lastFields.second : entry.field.zero,
            currentFields.second); rightDelta.has_value())
        {
            merged = entry.delta.integrate_field(merged, *rightDelta);
        }

        return merged;
    }

    auto build_merged_overlap(
        base::pair<iqsm::World> lastOverlaps,
        base::pair<iqsm::World> currentOverlaps,
        iqsm::Schema overlapSchema,
        const iqsm::SchemaObject::TypeSet& overlapTypes) -> iqsm::World
    {
        // Same contract as project_overlap: overlap slice only; provider is empty-schema ManagerCore host (not peers' managers).
        auto merged = make_overlap_slice_mutable(overlapSchema);

        for (const auto& typeId : overlapTypes) {
            const auto& entry = overlapSchema->aspects.at(typeId);
            const auto mergedField = merge_overlap_field(
                entry,
                {lastOverlaps.first->field(typeId), lastOverlaps.second->field(typeId)},
                {currentOverlaps.first->field(typeId), currentOverlaps.second->field(typeId)});

            if (mergedField == entry.field.zero) {
                continue;
            }

            merged->fields = merged->fields.insert(typeId, mergedField);
        }

        return iqsm::freeze(merged);
    }
}

namespace iqsm::agents {
    Collaboration::Collaboration(ref<Subsystem> left, ref<Subsystem> right)
        : peers{left, right}
        , lastSynced{left->access().current, right->access().current}
        , overlapTypes(SchemaObject::intersection(left->schema(), right->schema())->types())
    {}

    void Collaboration::sync() {
        const base::pair<Subsystem::Update> updates{peers.first->access(), peers.second->access()};

        const World leftCurrent = updates.first.current;
        const World rightCurrent = updates.second.current;
        const Schema overlapSchema = SchemaObject::intersection(leftCurrent->schema, rightCurrent->schema);

        if (overlapTypes.empty()) {
            lastSynced = {leftCurrent, rightCurrent};
            return;
        }

        const base::pair<World> lastOverlaps{
            project_overlap(lastSynced.first, overlapSchema, overlapTypes),
            project_overlap(lastSynced.second, overlapSchema, overlapTypes)};
        const base::pair<World> currentOverlaps{
            project_overlap(leftCurrent, overlapSchema, overlapTypes),
            project_overlap(rightCurrent, overlapSchema, overlapTypes)};

        const World mergedOverlap = build_merged_overlap(
            lastOverlaps,
            currentOverlaps,
            overlapSchema,
            overlapTypes);

        const Delta leftDelta = operations::make_delta(lastOverlaps.first, mergedOverlap);
        const Delta rightDelta = operations::make_delta(lastOverlaps.second, mergedOverlap);
        if (leftDelta->empty() && rightDelta->empty()) {
            lastSynced = {leftCurrent, rightCurrent};
            return;
        }

        // integrate() clones full leftCurrent/rightCurrent (same schema + resources Provider), then applies only overlap field deltas.
        const base::pair<World> next{
            leftDelta->empty()
                ? leftCurrent
                : operations::validate_smart(leftCurrent, operations::integrate(leftCurrent, leftDelta)),
            rightDelta->empty()
                ? rightCurrent
                : operations::validate_smart(rightCurrent, operations::integrate(rightCurrent, rightDelta))};

        if (next.first != leftCurrent) {
            updates.first.replace(next.first);
        }
        if (next.second != rightCurrent) {
            updates.second.replace(next.second);
        }

        const base::pair<World> finals{peers.first->access().current, peers.second->access().current};

        lastSynced = finals;
    }
}
