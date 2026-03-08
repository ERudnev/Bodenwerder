#include <iQSM/operations/integration.h>

#include <format>
#include <stdexcept>

namespace iqsm::ops {
    World integrate(World world, Delta delta) {
        // Reliable default: always validate the resulting world.
        return validate(integrate_raw(world, delta));
    }

    World integrate_raw(World world, Delta delta) {
        if (delta->empty()) { return world; }
        if (world->schema->empty()) { return world; }

        auto out = base::make_shared<WorldObject>(world->schema);
        out->fields = world->fields;

        for (const auto& kv : delta->fields) {
            const auto& typeId = kv.first;
            const auto& field_delta = kv.second;

            if (not world->schema->aspects.contains(typeId)) {
                throw std::runtime_error(std::format(
                    "integrate_raw(): delta contains type not in schema (hash={})",
                    typeId.hash_code()));
            }

            auto current = world->schema->aspects.at(typeId).zero;
            if (const auto* slot = world->fields.find(typeId); slot) {
                current = *slot;
            }

            const auto next = field_delta->integrate(current);
            out->fields = out->fields.insert(typeId, next);
        }

        return freeze(out);
    }

    World validate(World world) {
        // placeholder: validation logic will be implemented later
        return world;
    }

    Delta merge(Delta first, Delta second) {
        if (first->empty()) { return second; }
        if (second->empty()) { return first; }

        auto out = base::make_shared<iqsm::delta::Fields>();
        for (const auto& kv : first->fields) {
            out->fields = out->fields.insert(kv.first, kv.second);
        }

        for (const auto& kv : second->fields) {
            const auto& typeId = kv.first;
            const auto& rhs = kv.second;

            if (not out->fields.contains(typeId)) {
                out->fields = out->fields.insert(typeId, rhs);
                continue;
            }

            const auto lhs = out->fields.at(typeId);

            const auto merged = lhs->merge(rhs);
            out->fields = out->fields.insert(typeId, merged);
        }

        if (out->fields.empty()) { return ::iqsm::delta::empty(); }
        return freeze(out);
    }
}


