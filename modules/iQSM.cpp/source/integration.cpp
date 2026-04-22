#include <iQSM/operations/integration.h>

#include <format>
#include <set>
#include <stdexcept>
#include <vector>
#include <iQSM/repository/transactions/sequence.h>
#include <iQSM/repository/transactions/accumulator.h>

namespace iqsm::operations {
    namespace {
        using TypeId = iqsm::SchemaObject::TypeId;

        auto expand_required_by(Reading world, const std::set<TypeId>& touched) -> std::set<TypeId> {
            std::set<TypeId> out = touched;

            std::vector<TypeId> queue;
            queue.reserve(out.size());
            for (const auto& type_id : out) queue.push_back(type_id);

            for (size_t queue_index = 0; queue_index < queue.size(); ++queue_index) {
                const auto current_type = queue[queue_index];
                const auto& entry = world->schema->aspects.at(current_type);
                for (const auto& required_type : entry.required_by) {
                    if (out.insert(required_type).second) queue.push_back(required_type);
                }
            }
            return out;
        }

        auto topo_order_dependencies_first(Reading world, const std::set<TypeId>& subset) -> std::vector<TypeId> {
            // Kahn topological sort inside subset, where edges are: depender -> dependee (Entry::require).
            // We need dependencies first => if A requires B, B comes before A.
            std::map<TypeId, size_t> indegree;
            std::map<TypeId, std::vector<TypeId>> forward; // B -> [A] (dependency to dependers)

            for (const auto& type_id : subset) {
                indegree.emplace(type_id, size_t{0});
                forward.emplace(type_id, std::vector<TypeId>{});
            }

            for (const auto& type_id : subset) {
                const auto& required_types = world->schema->aspects.at(type_id).require;
                for (const auto& required_type : required_types) {
                    if (not subset.contains(required_type)) continue;
                    indegree.at(type_id) += 1;
                    forward.at(required_type).push_back(type_id);
                }
            }

            std::vector<TypeId> queue;
            for (const auto& indegree_entry : indegree) {
                if (indegree_entry.second == 0) queue.push_back(indegree_entry.first);
            }

            std::vector<TypeId> out;
            out.reserve(subset.size());
            for (size_t queue_index = 0; queue_index < queue.size(); ++queue_index) {
                const auto current_type = queue[queue_index];
                out.push_back(current_type);
                for (const auto& dependent_type : forward.at(current_type)) {
                    auto& dependent_indegree = indegree.at(dependent_type);
                    dependent_indegree -= 1;
                    if (dependent_indegree == 0) queue.push_back(dependent_type);
                }
            }

            // Fallback: in case of unexpected cycles, keep deterministic set order appended.
            if (out.size() != subset.size()) {
                for (const auto& type_id : subset) {
                    bool seen = false;
                    for (const auto& ordered_type : out) {
                        if (ordered_type == type_id) { seen = true; break; }
                    }
                    if (not seen) out.push_back(type_id);
                }
            }

            return out;
        }

        void apply_invariants_vector(Writing context, const iqsm::SchemaObject::Invariants::Layer& layer) {
            repo::Sequence transaction(context);

            for (const auto invariant : layer) {
                if (not invariant) continue;
                invariant(transaction);
            }
        }

        void apply_invariants_in_order(Writing context, const std::vector<TypeId>& order) {
            repo::Sequence transaction(context);

            for (const auto& type_id : order) {
                const auto& entry = transaction->schema->aspects.at(type_id);
                apply_invariants_vector(transaction, entry.invariants.structural);
                apply_invariants_vector(transaction, entry.invariants.logical);
            }
        }
    }

    World integrate(Reading world, Delta delta) {
        if (delta->empty()) { return world->share(); }
        if (world->schema->empty()) { return world->share(); }

        World base = world->share();
        auto out = base->clone();

        for (const auto& field_entry : delta->fields) {
            const auto& typeId = field_entry.first;
            const auto& field_delta = field_entry.second;

            if (not world->schema->aspects.contains(typeId)) {
                throw std::runtime_error(std::format(
                    "integrate(): delta contains type not in schema (hash={})",
                    typeId.hash_code()));
            }

            const auto& entry = world->schema->aspects.at(typeId);
            if (not entry.delta.integrate_field) {
                throw std::runtime_error(std::format(
                    "integrate(): schema entry has no integrate_field thunk (type={})",
                    entry.name));
            }

            const auto current = world->field(typeId);

            const auto next = entry.delta.integrate_field(current, field_delta);
            out->fields = out->fields.insert(typeId, next);
        }

        return freeze(out);
    }

    void validate_full(Writing context) {
        repo::Accumulator transaction(context);
        // validate everything (no delta context)
        if (transaction->schema->empty()) return;

        std::set<TypeId> all;
        for (const auto& aspect_entry : transaction->schema->aspects) all.insert(aspect_entry.first);
        const auto order = topo_order_dependencies_first(transaction, all);
        apply_invariants_in_order(transaction, order);
    }

    void validate_smart(Writing context, Reading validBeforeChanges) {
        repo::Sequence transaction(context);
        if (validBeforeChanges->schema != transaction->schema) {
            throw std::runtime_error("validate_smart(validBeforeChanges,changed): worlds must share the same schema handle");
        }

        if (validBeforeChanges == transaction) return;
        if (transaction->schema->empty()) return;

        std::set<TypeId> touched;
        for (const auto& aspect_entry : transaction->schema->aspects) {
            const auto& type_id = aspect_entry.first;

            const auto before = validBeforeChanges->field(type_id);
            const auto after = transaction->field(type_id);

            if (before != after) {
                touched.insert(type_id);
            }
        }

        if (touched.empty()) return;

        const auto affected = expand_required_by(transaction, touched);
        const auto order = topo_order_dependencies_first(transaction, affected);
        apply_invariants_in_order(transaction, order);
    }

    Delta make_delta(Reading from, Reading to) {
        if (from->schema != to->schema) {
            throw std::runtime_error("make_delta(from,to): worlds must share the same schema handle");
        }

        if (from == to) { return ::iqsm::delta::empty(); }
        if (from->schema->empty()) { return ::iqsm::delta::empty(); }

        auto out = base::make_shared<iqsm::delta::Fields>();

        for (const auto& aspect_entry : from->schema->aspects) {
            const auto& type_id = aspect_entry.first;
            const auto& entry = aspect_entry.second;

            const auto from_field = from->field(type_id);
            const auto to_field = to->field(type_id);

            if (not entry.delta.make_delta_field) {
                throw std::runtime_error(std::format(
                    "make_delta(from,to): schema entry has no make_delta_field thunk (type={})",
                    entry.name));
            }

            const auto field_delta = entry.delta.make_delta_field(from_field, to_field);
            if (not field_delta.has_value()) continue;

            out->fields.emplace(type_id, *field_delta);
        }

        if (out->fields.empty()) { return ::iqsm::delta::empty(); }
        return out;
    }
}


