#pragma once

#include <base/containers/ImmutableUnorderedSet.h>
#include <base/containers/ImmutableUnorderedMap.h>
#include <base/logging.h>
#include <iQSM/meta.h>
#include <iQSM/field.h>
#include <iQSM/_forwards.h>
#include <memory>
#include <optional>
#include <format>
#include <stdexcept>
#include <utility>

namespace iqsm {
    template<typename Result>
    struct DeltaAnd {
        Delta delta;
        Result receipt;
    };
}

namespace iqsm::delta {

    struct FieldDiffAbstract {
        using RuntimeTypeId = internals::Types::RuntimeId;
        virtual ~FieldDiffAbstract() = default;

        virtual iqsm::FieldAbstract::Ref integrate(iqsm::FieldAbstract::Ref) const = 0;
        virtual cref<FieldDiffAbstract> merge(cref<FieldDiffAbstract>) const = 0;
    };

    template<meta::Aspect Meta>
    struct FieldDiff final : Facet<Meta>, FieldDiffAbstract {
        using Id = iqsm::Id<Meta>;
        using Item = typename iqsm::Facet<Meta>::Item;

        using GlobalData = typename iqsm::Facet<Meta>::GlobalData;
        using Global = typename iqsm::Facet<Meta>::Global;

        struct Operation {
            std::optional<Item> add;
            std::optional<std::pair<Item, Item>> change; // {before, after}
            bool remove = false;
        };

        base::ImmutableUnorderedMap<Id, Operation> ops;
        std::optional<std::pair<Global, Global>> global_change; // {before, after}
        iqsm::FieldAbstract::Ref integrate(iqsm::FieldAbstract::Ref) const override;
        cref<FieldDiffAbstract> merge(cref<FieldDiffAbstract>) const override;
    private:
        Operation merge(const Operation& lhs, const Operation& rhs) const;
    };

    // Handles
    template<meta::Aspect Meta>
    using Field = cref<FieldDiff<Meta>>;
    using UField = cref<FieldDiffAbstract>;

    struct Fields {
        using Container = base::ImmutableUnorderedMap<FieldDiffAbstract::RuntimeTypeId, UField>;
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
        if (not op.add.has_value()) continue;

        if (container.contains(id)) {
            base::message("merge conflict resolved as last-wins");
            /* this check is disabled as reminder to apply merge strategies later.
            throw std::runtime_error(std::format(
                "delta::FieldDiff<{}>::integrate(): added already exists: {}",
                Facet<Meta>::typeName,
                id));
            */
        }
        container = container.insert(id, *op.add);
    }

    for (const auto& kv : ops) {
        const auto& id = kv.first;
        const auto& op = kv.second;
        if (not op.change.has_value()) continue;
        const auto& [before, after] = *op.change;

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
        if (not op.remove) continue;

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
iqsm::cref<iqsm::delta::FieldDiffAbstract> iqsm::delta::FieldDiff<Meta>::merge(iqsm::cref<FieldDiffAbstract> other) const {
    const auto rhs = base::shared_ref_cast<const FieldDiff<Meta>>(other);

    auto out = base::make_shared<FieldDiff<Meta>>();

    auto out_ops = ops;

    for (const auto& kv : rhs->ops) {
        const auto& id = kv.first;
        const auto merged = merge(out_ops.contains(id) ? out_ops.at(id) : Operation{}, kv.second);
        const bool empty = (not merged.add.has_value()) && (not merged.change.has_value()) && (not merged.remove);
        if (empty) {
            if (out_ops.contains(id)) out_ops = out_ops.erase(id);
            continue;
        }

        out_ops = out_ops.insert(id, merged);
    }

    out->ops = out_ops;

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
        out->global_change = std::pair<Global, Global>{lhs_before, rhs_after};
    } else if (rhs->global_change.has_value()) {
        out->global_change = rhs->global_change;
    } else if (global_change.has_value()) {
        out->global_change = global_change;
    }

    return iqsm::freeze(out);
}

template<iqsm::meta::Aspect Meta>
typename iqsm::delta::FieldDiff<Meta>::Operation iqsm::delta::FieldDiff<Meta>::merge(
    const Operation& lhs,
    const Operation& rhs) const
{
    Operation out = lhs;

    // Priority: remove -> add -> change
    if (rhs.remove) {
        if (out.add.has_value()) {
            // delete cancels creation completely (no "death" recorded)
            return Operation{};
        }
        out.change.reset();
        out.remove = true;
    }

    if (rhs.add.has_value()) {
        if (out.remove) {
            // death is unremovable
        } else if (out.add.has_value()) {
            // first creation wins
        } else if (out.change.has_value()) {
            // existence decided by first
        } else {
            out.add = *rhs.add;
        }
    }

    if (rhs.change.has_value()) {
        if (out.remove) {
            // death is unremovable
        } else if (out.add.has_value()) {
            // existence decided by first; ignore later modifications
        } else if (out.change.has_value()) {
            out.change = std::pair<Item, Item>{out.change->first, rhs.change->second};
        } else {
            out.change = *rhs.change;
        }
    }

    return out;
}