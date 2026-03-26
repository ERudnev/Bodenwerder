#include <iQSM/schema.h>
#include <iQSM/field.h>

#include <format>
#include <set>
#include <stdexcept>
#include <vector>

namespace iqsm {
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

                if (typeid(*lhs.field.zero) != typeid(*rhs.field.zero)) {
                    throw std::runtime_error(std::format(
                        "Schema::merge(): incompatible zero for aspect '{}'",
                        lhs.name));
                }

                if (lhs.invariants.structural != rhs.invariants.structural || lhs.invariants.logical != rhs.invariants.logical) {
                    throw std::runtime_error(std::format(
                        "Schema::merge(): incompatible invariants for aspect '{}'",
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


