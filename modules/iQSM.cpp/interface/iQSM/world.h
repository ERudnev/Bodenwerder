#pragma once

#include <memory>
#include <optional>
#include <stdexcept>
#include <typeindex>

#include <base/containers/ImmutableUnorderedMap.h>
#include <iQSM/schema.h>
#include <iQSM/field.h>
#include <iQSM/meta.h>
#include <iQSM/_forwards.h>

namespace iqsm {

    struct WorldObject {
        using Id = Identifier<WorldObject>;
        using Container = base::ImmutableUnorderedMap<FieldAbstract::RuntimeTypeId, FieldAbstract::Ref>;

        using TypeId = internals::Types::RuntimeId;

        const Id id;
        const Schema schema;
        Container fields;

        explicit WorldObject(Schema s) : id(Id::generate_random()), schema(s) {}

        template<meta::Aspect Meta> Field<Meta> field() const;

    private:
        void basis_required(TypeId rttid) const {
            if (not schema->aspects.contains(rttid)) { throw std::runtime_error("World: aspect is not in schema"); }
        }
    };

}

template<iqsm::meta::Aspect Meta>
iqsm::Field<Meta> iqsm::WorldObject::field() const {
    const TypeId rttid = Facet<Meta>::typeId;
    basis_required(rttid);

    auto untyped = schema->aspects.at(rttid).zero;
    if (const auto* slot = fields.find(rttid); slot) {
        untyped = *slot;
    }

    // TODO(iqsm): Type-integrity check. With non-null cref/ref, this should be a single throwing cast.
    return base::shared_ref_cast<const FieldObject<Meta>>(untyped);
}
