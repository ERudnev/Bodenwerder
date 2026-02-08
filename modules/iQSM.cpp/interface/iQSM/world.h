#pragma once

#include <memory>
#include <optional>
#include <stdexcept>
#include <typeindex>

#include <base/containers/ImmutableUnorderedMap.h>
#include <iQSM/dag.h>
#include <iQSM/field.h>
#include <iQSM/aspects.h>

namespace iqsm {

    struct WorldState {
        using Id = Identifier<WorldState>;
        using Container = base::ImmutableUnorderedMap<FieldUntyped::RuntimeTypeId, UField>;

        using TypeId = internals::Types::RuntimeId;

        const Id id;
        const Dag basis;
        Container fields;

        explicit WorldState(Dag basis_) : id(Id::generate_random()), basis(basis_) { required(basis, "WorldState: basis"); }

        template<Facet Meta> Field<Meta> field() const;

    private:
        void basis_required(TypeId rttid) const {
            if (not basis) { throw std::runtime_error("WorldState: basis is required"); }
            if (not basis->aspects.contains(rttid)) { throw std::runtime_error("WorldState: aspect is not in basis"); }
        }
    };

    // Handles
    using World = cref<WorldState>;


}

template<iqsm::Facet Meta>
iqsm::Field<Meta> iqsm::WorldState::field() const {
    const TypeId rttid = Aspect<Meta>::typeId;
    basis_required(rttid);

    UField untyped;
    if (const auto* slot = fields.find(rttid); slot and *slot) {
        untyped = *slot;
    } else {
        untyped = basis->aspects.at(rttid).zero;
    }

    auto typed = std::dynamic_pointer_cast<const FieldState<Meta>>(untyped);
    if (not typed) {
        throw std::runtime_error("WorldState::field(): invalid stored field type");
    }
    return typed;
}
