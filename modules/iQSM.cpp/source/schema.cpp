#include <iQSM/schema.h>

#include <format>
#include <stdexcept>

namespace iqsm {
    Schema SchemaObject::merge(Schema first, Schema second) {
        if (not first) { return second; }
        if (not second) { return first; }
        if (first.get() == second.get()) { return first; }

        Schema a = first;
        Schema b = second;
        if (std::less<const SchemaObject*>{}(b.get(), a.get())) { std::swap(a, b); }

        auto out = std::make_shared<SchemaObject>();
        out->aspects = a->aspects;

        for (const auto& [typeId, rhs] : b->aspects) {
            if (auto it = out->aspects.find(typeId); it != out->aspects.end()) {
                auto& lhs = it->second;

                if (lhs.require != rhs.require) {
                    throw std::runtime_error(std::format(
                        "Schema::merge(): incompatible requirements for aspect '{}'",
                        lhs.name));
                }

                if (not lhs.zero or not rhs.zero or typeid(*lhs.zero) != typeid(*rhs.zero)) {
                    throw std::runtime_error(std::format(
                        "Schema::merge(): incompatible zero for aspect '{}'",
                        lhs.name));
                }

                // Today we only store "own" (type-declared) invariants.
                // Future: allow invariants to be attached externally and merge them as a set-union (no duplicates).
                if (lhs.invariants.own != rhs.invariants.own) {
                    throw std::runtime_error(std::format(
                        "Schema::merge(): incompatible invariants for aspect '{}'",
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
}


