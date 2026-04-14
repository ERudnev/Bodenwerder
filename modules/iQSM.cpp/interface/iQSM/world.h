#pragma once

#include <stdexcept>

#include <base/containers/ImmutableUnorderedMap.h>

#include <iQSM/field.h>
#include <iQSM/identifier.h>
#include <iQSM/meta/aspect_id.h>
#include <iQSM/references.h>
#include <iQSM/schema.h>
#include <iQSM/types.h>

namespace iqsm {
    struct WorldObject {
        using Id = Identifier<WorldObject>;
        using TypeId = internals::Types::RuntimeId;
        using Container = base::ImmutableUnorderedMap<FieldAbstract::RuntimeTypeId, cref<FieldAbstract>>;

        const Id id;
        const Schema schema;
        Container fields;

        explicit WorldObject(Schema s) : id(Id::generate_random()), schema(s) {}

        cref<FieldAbstract> field(TypeId rttid) const;

        template<meta::Aspect Meta>
        Field<Meta> field() const;

    private:
        void basis_required(TypeId rttid) const {
            if (not schema->aspects.contains(rttid)) {
                throw std::runtime_error("World: aspect is not in schema");
            }
        }
    };
}

inline iqsm::cref<iqsm::FieldAbstract> iqsm::WorldObject::field(TypeId rttid) const {
    basis_required(rttid);

    auto untyped = schema->aspects.at(rttid).field.zero;
    if (const auto* slot = fields.find(rttid); slot) {
        untyped = *slot;
    }
    return untyped;
}

template<iqsm::meta::Aspect Meta>
iqsm::Field<Meta> iqsm::WorldObject::field() const {
    const TypeId rttid = types::aspectId<Meta>();
    return base::shared_ref_cast<const FieldData<Meta>>(field(rttid));
}

