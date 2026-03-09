#include <iQSM/operations/integration.h>

#include <format>
#include <set>
#include <stdexcept>
#include <vector>

#include <iQSM/operations/transaction.h>

namespace iqsm::ops {
    namespace {
        using TypeId = iqsm::SchemaObject::TypeId;

        auto expand_required_by(iqsm::World world, const std::set<TypeId>& touched) -> std::set<TypeId> {
            std::set<TypeId> out = touched;

            std::vector<TypeId> q;
            q.reserve(out.size());
            for (const auto& t : out) q.push_back(t);

            for (size_t i = 0; i < q.size(); ++i) {
                const auto cur = q[i];
                const auto& entry = world->schema->aspects.at(cur);
                for (const auto& dep : entry.required_by) {
                    if (out.insert(dep).second) q.push_back(dep);
                }
            }

            return out;
        }

        auto topo_order_dependencies_first(iqsm::World world, const std::set<TypeId>& subset) -> std::vector<TypeId> {
            // Kahn topological sort inside subset, where edges are: depender -> dependee (Entry::require).
            // We need dependencies first => if A requires B, B comes before A.
            std::map<TypeId, size_t> indegree;
            std::map<TypeId, std::vector<TypeId>> forward; // B -> [A] (dependency to dependers)

            for (const auto& t : subset) {
                indegree.emplace(t, size_t{0});
                forward.emplace(t, std::vector<TypeId>{});
            }

            for (const auto& t : subset) {
                const auto& req = world->schema->aspects.at(t).require;
                for (const auto& dep : req) {
                    if (not subset.contains(dep)) continue;
                    indegree.at(t) += 1;
                    forward.at(dep).push_back(t);
                }
            }

            std::vector<TypeId> q;
            for (const auto& kv : indegree) {
                if (kv.second == 0) q.push_back(kv.first);
            }

            std::vector<TypeId> out;
            out.reserve(subset.size());
            for (size_t i = 0; i < q.size(); ++i) {
                const auto cur = q[i];
                out.push_back(cur);
                for (const auto& next : forward.at(cur)) {
                    auto& d = indegree.at(next);
                    d -= 1;
                    if (d == 0) q.push_back(next);
                }
            }

            // Fallback: in case of unexpected cycles, keep deterministic set order appended.
            if (out.size() != subset.size()) {
                for (const auto& t : subset) {
                    bool seen = false;
                    for (const auto& x : out) {
                        if (x == t) { seen = true; break; }
                    }
                    if (not seen) out.push_back(t);
                }
            }

            return out;
        }

        auto validate_from_delta(iqsm::World world, iqsm::Delta delta) -> iqsm::World {
            if (delta->empty()) return world;
            if (world->schema->empty()) return world;

            std::set<TypeId> touched;
            for (const auto& kv : delta->fields) touched.insert(kv.first);

            const auto affected = expand_required_by(world, touched);
            const auto order = topo_order_dependencies_first(world, affected);

            iqsm::ops::Transaction tx{iqsm::World{world}};
            for (const auto& t : order) {
                const auto& entry = tx.current->schema->aspects.at(t);
                for (const auto fn : entry.invariants.own) {
                    if (not fn) continue;
                    tx.absorb(fn(tx.current));
                }
            }
            return tx.current;
        }
    }

    World integrate(World world, Delta delta) {
        // Reliable default: always validate the resulting world.
        return validate_from_delta(integrate_raw(world, delta), delta);
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
        // helper: validate everything (no delta context)
        if (world->schema->empty()) return world;

        std::set<TypeId> all;
        for (const auto& kv : world->schema->aspects) all.insert(kv.first);
        const auto order = topo_order_dependencies_first(world, all);

        iqsm::ops::Transaction tx{iqsm::World{world}};
        for (const auto& t : order) {
            const auto& entry = tx.current->schema->aspects.at(t);
            for (const auto fn : entry.invariants.own) {
                if (not fn) continue;
                tx.absorb(fn(tx.current));
            }
        }
        return tx.current;
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


