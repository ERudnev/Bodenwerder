#pragma once

#include <base/logging.h>
#include <iQSM/meta.h>
#include <iQSM/field.h>
#include <iQSM/_forwards.h>
#include <memory>
#include <optional>
#include <unordered_map>
#include <format>
#include <stdexcept>
#include <utility>

namespace iqsm::delta {

    struct FieldDiffAbstract {
        using RuntimeTypeId = internals::Types::RuntimeId;
        virtual ~FieldDiffAbstract() = default;

        virtual bool empty() const = 0;
        virtual ref<FieldDiffAbstract> clone() const = 0;
        virtual void merge(cref<FieldDiffAbstract>) = 0; // apply RHS into *this
        virtual iqsm::FieldAbstract::Ref integrate(iqsm::FieldAbstract::Ref) const = 0;
    };

    template<meta::Aspect Meta>
    struct FieldDiff final : Facet<Meta>, FieldDiffAbstract {
        using Id = iqsm::Id<Meta>;
        using Item = typename iqsm::Facet<Meta>::Item;

        using GlobalData = typename iqsm::Facet<Meta>::GlobalData;
        using Global = typename iqsm::Facet<Meta>::Global;

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

        bool empty() const override { return ops.empty() && not global_change.has_value(); }
        ref<FieldDiffAbstract> clone() const override { return base::make_shared<FieldDiff<Meta>>(*this); }
        void merge(cref<FieldDiffAbstract>) override;
        iqsm::FieldAbstract::Ref integrate(iqsm::FieldAbstract::Ref) const override;
    private:
        Operation merge(const Operation& lhs, const Operation& rhs) const;
    };

    // Handles
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
}

template<iqsm::meta::Aspect Meta>
iqsm::FieldAbstract::Ref iqsm::delta::FieldDiff<Meta>::integrate(iqsm::FieldAbstract::Ref current) const {
    if (ops.empty() && not global_change.has_value()) { return current; }

    // TODO(iqsm): These casts are type-integrity checks (not null-checks). When cref/ref becomes a non-null pointer,
    // decide on the standard cast API here (e.g. `cast<T>()` throws, `try_cast<T>()` returns optional).
    auto typed = base::shared_ref_cast<const iqsm::FieldObject<Meta>>(current);

    auto container = typed->container;

    // Apply in the same phase order as the previous representation:
    // add -> change -> remove.
    for (const auto& kv : ops) {
        const auto& id = kv.first;
        const auto& op = kv.second;
        if (not op.is_add()) continue;

        if (container.contains(id)) {
            base::message("merge conflict resolved as last-wins");
            /* this check is disabled as reminder to apply merge strategies later.
            throw std::runtime_error(std::format(
                "delta::FieldDiff<{}>::integrate(): added already exists: {}",
                Facet<Meta>::typeName,
                id));
            */
        }
        container = container.insert(id, *op.after);
    }

    for (const auto& kv : ops) {
        const auto& id = kv.first;
        const auto& op = kv.second;
        if (not op.is_chg()) continue;
        const auto& before = *op.before;
        const auto& after = *op.after;

        if (not container.contains(id)) {
            base::message("merge conflict resolved as last-wins");
            /* this check is disabled as reminder to apply merge strategies later.
            throw std::runtime_error(std::format(
                "delta::FieldDiff<{}>::integrate(): changed missing: {}",
                Facet<Meta>::typeName,
                id));
            */
            container = container.insert(id, after);
            continue;
        }
        
        if (container.at(id) != before) {
            base::message("merge conflict resolved as last-wins");
            /* this check is disabled as reminder to apply merge strategies later.
            throw std::runtime_error(std::format(
                "delta::FieldDiff<{}>::integrate(): before mismatch: {}",
                Facet<Meta>::typeName,
                id));
            */
        }
        container = container.insert(id, after);
    }

    for (const auto& kv : ops) {
        const auto& id = kv.first;
        const auto& op = kv.second;
        if (not op.is_del()) continue;

        if (not container.contains(id)) {
            base::message("merge conflict resolved as last-wins");
            /* this check is disabled as reminder to apply merge strategies later.
            throw std::runtime_error(std::format(
                "delta::FieldDiff<{}>::integrate(): deleted missing: {}",
                Facet<Meta>::typeName,
                id));
            */
            continue;
        }
        container = container.erase(id);
    }

    auto out = base::make_shared<iqsm::FieldObject<Meta>>();
    out->container = container;
    out->global = typed->global;

    if (global_change.has_value()) {
        const auto& [before, after] = *global_change;
        if (typed->global != before) {
            base::message("merge conflict resolved as last-wins");
            /* this check is disabled as reminder to apply merge strategies later.
            throw std::runtime_error(std::format(
                "delta::FieldDiff<{}>::integrate(): global before mismatch",
                Facet<Meta>::typeName));
            */
        }
        out->global = after;
    }
    return iqsm::freeze(out);
}

template<iqsm::meta::Aspect Meta>
void iqsm::delta::FieldDiff<Meta>::merge(iqsm::cref<FieldDiffAbstract> other) {
    const auto rhs = base::shared_ref_cast<const FieldDiff<Meta>>(other);

    for (const auto& kv : rhs->ops) {
        const auto& id = kv.first;
        const auto& rhs_op = kv.second;

        const auto it = ops.find(id);
        const auto lhs_op = (it == ops.end()) ? Operation{} : it->second;
        const auto merged_op = merge(lhs_op, rhs_op);

        if (merged_op.is_noop()) {
            if (it != ops.end()) { ops.erase(id); }
            continue;
        }

        ops.insert_or_assign(id, merged_op);
    }

    if (global_change.has_value() && rhs->global_change.has_value()) {
        const auto& [lhs_before, lhs_after] = *global_change;
        const auto& [rhs_before, rhs_after] = *rhs->global_change;
        if (lhs_after != rhs_before) {
            base::message("merge conflict resolved as last-wins");
            /* this check is disabled as reminder to apply merge strategies later.
            throw std::runtime_error(std::format(
                "delta::FieldDiff<{}>::merge(): global before mismatch",
                Facet<Meta>::typeName));
            */
        }
        global_change = std::pair<Global, Global>{lhs_before, rhs_after};
    } else if (rhs->global_change.has_value()) {
        global_change = rhs->global_change;
    }
}

template<iqsm::meta::Aspect Meta>
typename iqsm::delta::FieldDiff<Meta>::Operation iqsm::delta::FieldDiff<Meta>::merge(
    const Operation& lhs,
    const Operation& rhs) const
{
    // Merge policy for unique ids (single causal history per Id):
    // - add is always the first possible event for a given Id
    // - del is always after add/chg (no resurrection for the same Id)
    // - the only case where we treat the RHS as "later" is chg+chg (LWW for value).
    //
    // This makes merge mostly order-insensitive for {add, chg, del}, but still deterministic.
    if (rhs.is_noop()) return lhs;
    if (lhs.is_noop()) return rhs;

    const auto report = []() {
        base::message("merge conflict resolved by unique-id policy");
    };

    // chg + chg : last-wins for final value, keep earliest known "before".
    if (lhs.is_chg() && rhs.is_chg()) {
        if (lhs.after && rhs.before && *lhs.after != *rhs.before) {
            report();
        }
        return Operation{lhs.before, rhs.after};
    }

    // Any delete dominates (delete is terminal for an Id).
    if (lhs.is_del() || rhs.is_del()) {
        const auto& del   = lhs.is_del() ? lhs : rhs;
        const auto& other = lhs.is_del() ? rhs : lhs;

        auto before = del.before;

        // If the delete doesn't carry the state being deleted, try to recover it
        // from the other operation (delete must happen after add/chg).
        if (!before) {
            if (other.is_chg()) before = other.after;
            else if (other.is_add()) before = other.after;
            else if (other.is_del()) before = other.before;
        } else {
            if (other.is_chg() && other.after && *before != *other.after) report();
            if (other.is_add() && other.after && *before != *other.after) report();
            if (other.is_del() && other.before && *before != *other.before) report();
        }

        return Operation{before, std::nullopt};
    }

    // Any add is earlier than any change for a given Id (can't re-create the same Id).
    if (lhs.is_add() || rhs.is_add()) {
        const auto& add   = lhs.is_add() ? lhs : rhs;
        const auto& other = lhs.is_add() ? rhs : lhs;

        // add + add: logically impossible for unique ids unless it's a duplicate.
        // Keep RHS value to remain deterministic within FieldDiff::merge() ordering.
        if (other.is_add()) {
            if (add.after && other.after && *add.after != *other.after) {
                report();
            }
            return Operation{std::nullopt, rhs.after};
        }

        // add + chg: normalize to {null, final} (as if "add then chg").
        if (other.is_chg()) {
            if (add.after && other.before && *add.after != *other.before) {
                report();
            }
            return Operation{std::nullopt, other.after};
        }

        // Should be unreachable: del is handled above, noop is handled at the top.
        report();
        return Operation{std::nullopt, rhs.after};
    }

    // Remaining combinations should have been handled above; fallback to RHS for determinism.
    report();
    return rhs;
}