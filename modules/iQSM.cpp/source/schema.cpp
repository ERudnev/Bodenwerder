#include <iQSM/schema.h>
#include <iQSM/field.h>

#include <format>
#include <set>
#include <stdexcept>
#include <vector>

namespace iqsm {
    auto SchemaObject::types() const -> TypeSet{
        TypeSet out;
        for (const auto& [id, _] : aspects) {
            out.insert(id);
        }
        return out;
    }

    Schema SchemaObject::merge(Schema first, Schema second) {
        if (first.get() == second.get()) { return first; }
        if (first->empty()) { return second; }
        if (second->empty()) { return first; }

        Schema a = first;
        Schema b = second;
        if (std::less<const SchemaObject*>{}(b.get(), a.get())) { std::swap(a, b); }

        auto out = base::make_shared<SchemaObject>();
        out->aspects = a->aspects;

        for (const auto& [typeId, rhs] : b->aspects) {
            if (auto it = out->aspects.find(typeId); it != out->aspects.end()) {
                auto& lhs = it->second;

                if (lhs.require != rhs.require) {
                    throw std::runtime_error(std::format(
                        "Schema::merge(): incompatible requirements for aspect '{}'",
                        lhs.name));
                }

                if (lhs.invariants.structural != rhs.invariants.structural || lhs.invariants.logical != rhs.invariants.logical) {
                    throw std::runtime_error(std::format(
                        "Schema::merge(): incompatible invariants for aspect '{}'",
                        lhs.name));
                }

                if (lhs.resource.create_slot != rhs.resource.create_slot) {
                    throw std::runtime_error(std::format(
                        "Schema::merge(): incompatible resource runtime for aspect '{}'",
                        lhs.name));
                }

                if (lhs.delta.make_delta_field != rhs.delta.make_delta_field) {
                    throw std::runtime_error(std::format(
                        "Schema::merge(): incompatible delta ops for aspect '{}'",
                        lhs.name));
                }
            } else {
                out->aspects.emplace(typeId, rhs);
            }
        }

        out->check_closed();
        out->update_required_by();
        return freeze(out);
    }

    Schema SchemaObject::intersection(Schema first, Schema second) {
        if (first.get() == second.get()) { return first; }
        if (first->empty() || second->empty()) { return freeze(base::make_shared<SchemaObject>()); }

        auto out = base::make_shared<SchemaObject>();

        for (const auto& [typeId, lhs] : first->aspects) {
            const auto rhs_it = second->aspects.find(typeId);
            if (rhs_it == second->aspects.end()) continue;

            const auto& rhs = rhs_it->second;

            if (lhs.name != rhs.name) {
                throw std::runtime_error(std::format(
                    "Schema::intersection(): incompatible names for shared aspect (hash={})",
                    typeId.hash_code()));
            }

            if (lhs.invariants.structural != rhs.invariants.structural || lhs.invariants.logical != rhs.invariants.logical) {
                throw std::runtime_error(std::format(
                    "Schema::intersection(): incompatible invariants for aspect '{}'",
                    lhs.name));
            }

            if (lhs.resource.create_slot != rhs.resource.create_slot) {
                throw std::runtime_error(std::format(
                    "Schema::intersection(): incompatible resource runtime for aspect '{}'",
                    lhs.name));
            }

            if (lhs.delta.make_delta_field != rhs.delta.make_delta_field
                || lhs.delta.integrate_field != rhs.delta.integrate_field
                || lhs.delta.empty != rhs.delta.empty
                || lhs.delta.absorb != rhs.delta.absorb) {
                throw std::runtime_error(std::format(
                    "Schema::intersection(): incompatible delta ops for aspect '{}'",
                    lhs.name));
            }

            out->aspects.emplace(typeId, lhs);
        }

        for (auto& [_, entry] : out->aspects) {
            for (auto it = entry.require.begin(); it != entry.require.end();) {
                if (out->aspects.contains(*it)) {
                    ++it;
                } else {
                    it = entry.require.erase(it);
                }
            }
        }

        out->check_closed();
        out->update_required_by();
        return freeze(out);
    }

    void SchemaObject::check_closed() const {
        for (const auto& [a, e] : aspects) {
            for (const auto& dep : e.require) {
                if (aspects.find(dep) == aspects.end()) {
                    throw std::runtime_error(std::format(
                        "Schema is not closed: missing dependency (hash={}) (required by '{}')",
                        dep.hash_code(),
                        e.name));
                }
            }
        }
    }

    void SchemaObject::update_required_by() {
        for (auto& [_, e] : aspects) { e.required_by.clear(); }
        for (const auto& [a, e] : aspects) {
            for (const auto& dep : e.require) {
                aspects.at(dep).required_by.insert(a);
            }
        }
    }

    bool SchemaObject::depends(TypeId depender, TypeId dependee) const {
        if (depender == dependee) return false;
        if (not aspects.contains(depender)) return false;
        if (not aspects.contains(dependee)) return false;

        const auto& start = aspects.at(depender).require;
        if (start.contains(dependee)) return true; // direct fast-path

        std::vector<TypeId> q;
        q.reserve(start.size());

        std::set<TypeId> visited;
        visited.insert(depender);

        for (const auto& d : start) {
            if (visited.insert(d).second) q.push_back(d);
        }

        for (decltype(q.size()) i = 0; i < q.size(); ++i) {
            const auto cur = q[i];
            const auto& req = aspects.at(cur).require;
            if (req.contains(dependee)) return true;
            for (const auto& d : req) {
                if (visited.insert(d).second) q.push_back(d);
            }
        }

        return false;
    }
}


