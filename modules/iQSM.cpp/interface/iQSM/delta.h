#pragma once

#include <base/containers/ImmutableUnorderedSet.h>
#include <base/containers/ImmutableUnorderedMap.h>
#include <iQSM/aspects.h>
#include <iQSM/field.h>
#include <format>
#include <stdexcept>

namespace iqsm {

    // local forward
    namespace delta { struct WorldState; }

    // root-level semantics:
    using Delta = cref<delta::WorldState>;

    template<typename Result>
    struct DeltaAnd {
        Delta delta;
        Result receipt;
    };
}

namespace iqsm::delta {

    struct FieldUntyped {
        using RuntimeTypeId = internals::Types::RuntimeId;
        virtual ~FieldUntyped() = default;

        virtual iqsm::UField integrate(iqsm::UField) const = 0;
        virtual cref<FieldUntyped> merge(cref<FieldUntyped>) const = 0;
    };

    template<Facet Meta>
    struct FieldState final : Aspect<Meta>, FieldUntyped {
        using A = iqsm::Aspect<Meta>;
        using ItemId = typename A::ItemId;
        using Item = typename A::Item;
        
        struct Change {
            Item before;
            Item after;
        };

        base::ImmutableUnorderedMap<ItemId, Item> added;
        base::ImmutableUnorderedMap<ItemId, Change> changed;
        base::ImmutableUnorderedSet<ItemId> deleted;
        iqsm::UField integrate(iqsm::UField) const override;
        cref<FieldUntyped> merge(cref<FieldUntyped>) const override;
    };

    // Handles
    template<Facet Meta>
    using Field = cref<FieldState<Meta>>;
    using UField = cref<FieldUntyped>;

    struct WorldState {
        using Container = base::ImmutableUnorderedMap<FieldUntyped::RuntimeTypeId, UField>;
        Container fields;
    };
}

template<iqsm::Facet Meta>
iqsm::UField iqsm::delta::FieldState<Meta>::integrate(iqsm::UField current) const {
    if (added.empty() and changed.empty() and deleted.empty()) { return current; }

    auto typed = std::dynamic_pointer_cast<const iqsm::FieldState<Meta>>(current);
    if (not typed) {
        throw std::runtime_error(std::format(
            "delta::FieldState<{}>::integrate(): invalid field type",
            Aspect<Meta>::typeName));
    }

    auto container = typed->container;

    for (const auto& kv : added) {
        if (container.contains(kv.first)) {
            throw std::runtime_error(std::format(
                "delta::FieldState<{}>::integrate(): added already exists: {}",
                Aspect<Meta>::typeName,
                kv.first));
        }
        container = container.insert(kv.first, kv.second);
    }
    for (const auto& kv : changed) {
        if (not container.contains(kv.first)) {
            throw std::runtime_error(std::format(
                "delta::FieldState<{}>::integrate(): changed missing: {}",
                Aspect<Meta>::typeName,
                kv.first));
        }
        if (kv.second.before and container.at(kv.first) != kv.second.before) {
            throw std::runtime_error(std::format(
                "delta::FieldState<{}>::integrate(): before mismatch: {}",
                Aspect<Meta>::typeName,
                kv.first));
        }
        container = container.insert(kv.first, kv.second.after);
    }
    for (const auto& id : deleted) {
        if (not container.contains(id)) {
            throw std::runtime_error(std::format(
                "delta::FieldState<{}>::integrate(): deleted missing: {}",
                Aspect<Meta>::typeName,
                id));
        }
        container = container.erase(id);
    }

    auto out = std::make_shared<iqsm::FieldState<Meta>>();
    out->container = container;
    return std::static_pointer_cast<const iqsm::FieldUntyped>(iqsm::freeze(out));
}

template<iqsm::Facet Meta>
iqsm::cref<iqsm::delta::FieldUntyped> iqsm::delta::FieldState<Meta>::merge(iqsm::cref<FieldUntyped> other) const {
    const auto* rhs = dynamic_cast<const FieldState<Meta>*>(other.get());
    if (not rhs) {
        throw std::runtime_error(std::format(
            "delta::FieldState<{}>::merge(): incompatible field delta type",
            Aspect<Meta>::typeName));
    }

    auto out = std::make_shared<FieldState<Meta>>();
    auto added_out = added;
    auto changed_out = changed;
    auto deleted_out = deleted;

    // Apply deletes from second
    for (const auto& id : rhs->deleted) {
        if (added_out.contains(id)) {
            added_out = added_out.erase(id);
            if (changed_out.contains(id)) { changed_out = changed_out.erase(id); }
            if (deleted_out.contains(id)) { deleted_out = deleted_out.erase(id); }
            continue;
        }
        if (changed_out.contains(id)) { changed_out = changed_out.erase(id); }
        deleted_out = deleted_out.insert(id);
    }

    // Apply adds from second
    for (const auto& kv : rhs->added) {
        const auto& id = kv.first;
        const auto& item = kv.second;
        if (deleted_out.contains(id)) { continue; } // death is unremovable
        if (added_out.contains(id)) { continue; } // first creation wins
        if (changed_out.contains(id)) { continue; } // existence decided by first
        added_out = added_out.insert(id, item);
    }

    // Apply changes from second
    for (const auto& kv : rhs->changed) {
        const auto& id = kv.first;
        const auto& c2 = kv.second;

        if (deleted_out.contains(id)) { continue; }
        if (added_out.contains(id)) { continue; } // existence decided by first; ignore later modifications

        if (changed_out.contains(id)) {
            const auto c1 = changed_out.at(id);
            changed_out = changed_out.insert(id, Change{c1.before, c2.after});
        } else {
            changed_out = changed_out.insert(id, c2);
        }
    }

    out->added = added_out;
    out->changed = changed_out;
    out->deleted = deleted_out;

    return std::static_pointer_cast<const FieldUntyped>(iqsm::freeze(out));
}