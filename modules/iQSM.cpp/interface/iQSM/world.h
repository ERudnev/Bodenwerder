#pragma once

#include <memory>
#include <optional>
#include <stdexcept>
#include <typeindex>

#include <base/containers/ImmutableUnorderedMap.h>
#include <iQSM/schema.h>
#include <iQSM/field.h>
#include <iQSM/aspects.h>
#include <iQSM/_forwards.h>

namespace iqsm {

    struct WorldObject {
        using Id = Identifier<WorldObject>;
        using Container = base::ImmutableUnorderedMap<FieldAbstract::RuntimeTypeId, FieldAbstract::Ref>;

        using TypeId = internals::Types::RuntimeId;

        const Id id;
        const Schema schema;
        Container fields;

        explicit WorldObject(Schema s) : id(Id::generate_random()), schema(s) { required(schema, "World: schema"); }

        template<Facet Meta> Field<Meta> field() const;

    private:
        void basis_required(TypeId rttid) const {
            if (not schema) { throw std::runtime_error("World: schema is required"); }
            if (not schema->aspects.contains(rttid)) { throw std::runtime_error("World: aspect is not in schema"); }
        }
    };

}

template<iqsm::Facet Meta>
iqsm::Field<Meta> iqsm::WorldObject::field() const {
    const TypeId rttid = Aspect<Meta>::typeId;
    basis_required(rttid);

    FieldAbstract::Ref untyped;
    if (const auto* slot = fields.find(rttid); slot and *slot) {
        untyped = *slot;
    } else {
        untyped = schema->aspects.at(rttid).zero;
    }

    auto typed = std::dynamic_pointer_cast<const FieldObject<Meta>>(untyped);
    if (not typed) {
        throw std::runtime_error("World::field(): invalid stored field type");
    }
    return typed;
}
