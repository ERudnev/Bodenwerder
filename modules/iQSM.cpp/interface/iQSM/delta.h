#pragma once

#include <optional>
#include <stdexcept>
#include <typeinfo>
#include <unordered_map>
#include <utility>

#include <base/logging.h>

#include <iQSM/_forwards.h>
#include <iQSM/field.h>
#include <iQSM/meta/aspect_id.h>
#include <iQSM/meta/concepts.h>
#include <iQSM/meta/facade.h>
#include <iQSM/meta/global.h>
#include <iQSM/references.h>
#include <iQSM/types.h>

namespace iqsm::delta {
    struct FieldDiffAbstract {
        using RuntimeTypeId = internals::Types::RuntimeId;
        virtual ~FieldDiffAbstract() = default;
    };

    template<meta::Aspect Meta>
    struct FieldDiff final : FieldDiffAbstract {
        using Id = ::iqsm::Id<Meta>;
        using Quantum = ::iqsm::Quantum<Meta>;
        using Item = ::iqsm::Item<Meta>;

        using GlobalData = ::iqsm::meta::GlobalData<Meta>;
        using Global = ::iqsm::meta::Global<Meta>;

        struct Operation {
            std::optional<Item> before;
            std::optional<Item> after;

            bool is_noop() const { return !before && !after; }
            bool is_add()  const { return !before &&  after; }
            bool is_del()  const { return  before && !after; }
            bool is_chg()  const { return  before &&  after; }
        };

        std::unordered_map<Id, Operation> ops;
        std::optional<std::pair<Global, Global>> global_change; // {before, after}

        bool empty() const { return ops.empty() && not global_change.has_value(); }
        ref<FieldDiffAbstract> clone() const { return base::make_shared<FieldDiff<Meta>>(*this); }
        void absorb(const FieldDiff& rhs);
        cref<FieldAbstract> integrate(cref<FieldAbstract> current) const;

    private:
        Operation merge_ops(const Operation& lhs, const Operation& rhs) const;
    };

    template<meta::Aspect Meta>
    using Field = cref<FieldDiff<Meta>>;

    using UField = cref<FieldDiffAbstract>;

    struct Fields {
        using Container = std::unordered_map<FieldDiffAbstract::RuntimeTypeId, UField>;
        Container fields;
        bool empty() const { return fields.empty(); }
    };

    inline iqsm::Delta empty() {
        static const auto singleton = base::make_shared<const Fields>();
        return singleton;
    }
} // namespace iqsm::delta

template<iqsm::meta::Aspect Meta>
iqsm::cref<iqsm::FieldAbstract> iqsm::delta::FieldDiff<Meta>::integrate(iqsm::cref<iqsm::FieldAbstract> current) const {
    if (ops.empty() && not global_change.has_value()) { return current; }

    auto typed = base::shared_ref_cast<const iqsm::FieldData<Meta>>(current);
    auto container = typed->container;

    // Apply in phase order: add -> change -> remove.
    for (const auto& kv : ops) {
        const auto& id = kv.first;
        const auto& op = kv.second;
        if (not op.is_add()) continue;

        if (container.contains(id)) { base::message("delta integrate conflict resolved as last-wins (aspect={})", typeid(Meta).name()); }
        container = container.insert(id, *op.after);
    }

    for (const auto& kv : ops) {
        const auto& id = kv.first;
        const auto& op = kv.second;
        if (not op.is_chg()) continue;

        const auto& before = *op.before;
        const auto& after = *op.after;

        if (not container.contains(id)) { base::message("delta integrate conflict resolved as last-wins (aspect={})", typeid(Meta).name()); container = container.insert(id, after); continue; }

        if (container.at(id) != before) { base::message("delta integrate conflict resolved as last-wins (aspect={})", typeid(Meta).name()); }
        container = container.insert(id, after);
    }

    for (const auto& kv : ops) {
        const auto& id = kv.first;
        const auto& op = kv.second;
        if (not op.is_del()) continue;

        if (not container.contains(id)) { base::message("delta integrate conflict resolved as last-wins (aspect={})", typeid(Meta).name()); continue; }
        container = container.erase(id);
    }

    auto out = base::make_shared<iqsm::FieldData<Meta>>();
    out->container = container;
    out->global = typed->global;

    if (global_change.has_value()) {
        const auto& [before, after] = *global_change;
        if (typed->global != before) { base::message("delta integrate conflict resolved as last-wins (aspect={})", typeid(Meta).name()); }
        out->global = after;
    }

    return iqsm::freeze(out);
}

template<iqsm::meta::Aspect Meta>
void iqsm::delta::FieldDiff<Meta>::absorb(const FieldDiff& rhs) {
    for (const auto& kv : rhs.ops) {
        const auto& id = kv.first;
        const auto& rhs_op = kv.second;

        const auto it = ops.find(id);
        const auto lhs_op = (it == ops.end()) ? Operation{} : it->second;
        const auto merged = merge_ops(lhs_op, rhs_op);

        if (merged.is_noop()) {
            if (it != ops.end()) { ops.erase(id); }
            continue;
        }

        ops.insert_or_assign(id, merged);
    }

    if (global_change.has_value() && rhs.global_change.has_value()) {
        const auto& [lhs_before, lhs_after] = *global_change;
        const auto& [rhs_before, rhs_after] = *rhs.global_change;
        if (lhs_after != rhs_before) { base::message("delta absorb conflict resolved as last-wins (aspect={})", typeid(Meta).name()); }
        global_change = std::pair<Global, Global>{lhs_before, rhs_after};
    } else if (rhs.global_change.has_value()) {
        global_change = rhs.global_change;
    }
}

template<iqsm::meta::Aspect Meta>
typename iqsm::delta::FieldDiff<Meta>::Operation
iqsm::delta::FieldDiff<Meta>::merge_ops(const Operation& lhs, const Operation& rhs) const {
    if (rhs.is_noop()) return lhs;
    if (lhs.is_noop()) return rhs;

    // chg + chg : last-wins for final value, keep earliest known "before".
    if (lhs.is_chg() && rhs.is_chg()) {
        if (lhs.after && rhs.before && *lhs.after != *rhs.before) { base::message("delta absorb conflict resolved by unique-id policy (aspect={})", typeid(Meta).name()); }
        return Operation{lhs.before, rhs.after};
    }

    // Any delete dominates (delete is terminal for an Id).
    if (lhs.is_del() || rhs.is_del()) {
        const auto& del   = lhs.is_del() ? lhs : rhs;
        const auto& other = lhs.is_del() ? rhs : lhs;

        auto before = del.before;

        // If delete doesn't carry the state being deleted, try to recover it.
        if (!before) {
            if (other.is_chg()) before = other.after;
            else if (other.is_add()) before = other.after;
            else if (other.is_del()) before = other.before;
        } else {
            if (other.is_chg() && other.after && *before != *other.after) { base::message("delta absorb conflict resolved by unique-id policy (aspect={})", typeid(Meta).name()); }
            if (other.is_add() && other.after && *before != *other.after) { base::message("delta absorb conflict resolved by unique-id policy (aspect={})", typeid(Meta).name()); }
            if (other.is_del() && other.before && *before != *other.before) { base::message("delta absorb conflict resolved by unique-id policy (aspect={})", typeid(Meta).name()); }
        }

        return Operation{before, std::nullopt};
    }

    // Any add is earlier than any change for a given Id.
    if (lhs.is_add() || rhs.is_add()) {
        const auto& add   = lhs.is_add() ? lhs : rhs;
        const auto& other = lhs.is_add() ? rhs : lhs;

        // add + add: keep RHS value (deterministic within absorb ordering).
        if (other.is_add()) {
            if (add.after && other.after && *add.after != *other.after) { base::message("delta absorb conflict resolved by unique-id policy (aspect={})", typeid(Meta).name()); }
            return Operation{std::nullopt, rhs.after};
        }

        // add + chg: normalize to {null, final}.
        if (other.is_chg()) {
            if (add.after && other.before && *add.after != *other.before) { base::message("delta absorb conflict resolved by unique-id policy (aspect={})", typeid(Meta).name()); }
            return Operation{std::nullopt, other.after};
        }

        base::message("delta absorb conflict resolved by unique-id policy (aspect={})", typeid(Meta).name());
        return Operation{std::nullopt, rhs.after};
    }

    base::message("delta absorb conflict resolved by unique-id policy (aspect={})", typeid(Meta).name());
    return rhs;
}

