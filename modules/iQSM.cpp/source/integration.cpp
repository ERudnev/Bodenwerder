#include <iQSM/operations/integration.h>

#include <format>
#include <stdexcept>

namespace iqsm::ops {
    World integrate(World world, Delta delta) {
        // Reliable default: always validate the resulting world.
        return validate(integrate_raw(world, delta));
    }

    World integrate_raw(World world, Delta delta) {
        if (not delta) { return world; }
        if (not world) { return nullptr; }
        if (delta->fields.empty()) { return world; }
        required(world->basis, "integrate_raw(): world basis");

        auto out = std::make_shared<WorldState>(world->basis);
        out->fields = world->fields;

        for (const auto& kv : delta->fields) {
            const auto& typeId = kv.first;
            const auto& field_delta = kv.second;
            if (not field_delta) { continue; }

            if (not world->basis->aspects.contains(typeId)) {
                throw std::runtime_error(std::format(
                    "integrate_raw(): delta contains type not in basis: '{}'",
                    typeId.name()));
            }

            iqsm::UField current;
            if (const auto* slot = world->fields.find(typeId); slot and *slot) {
                current = *slot;
            } else {
                current = world->basis->aspects.at(typeId).zero;
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
        if (not first) { return second; }
        if (not second) { return first; }
        if (first->fields.empty()) { return second; }
        if (second->fields.empty()) { return first; }

        auto out = std::make_shared<iqsm::delta::WorldState>();
        for (const auto& kv : first->fields) {
            out->fields = out->fields.insert(kv.first, kv.second);
        }

        for (const auto& kv : second->fields) {
            const auto& typeId = kv.first;
            const auto& rhs = kv.second;
            if (not rhs) { continue; }

            if (not out->fields.contains(typeId)) {
                out->fields = out->fields.insert(typeId, rhs);
                continue;
            }

            const auto lhs = out->fields.at(typeId);
            if (not lhs) {
                out->fields = out->fields.insert(typeId, rhs);
                continue;
            }

            const auto merged = lhs->merge(rhs);
            out->fields = out->fields.insert(typeId, merged);
        }

        if (out->fields.empty()) { return nullptr; }
        return freeze(out);
    }
}


