#pragma once

#include <stdexcept>

#include <base/containers/ImmutableUnorderedMap.h>

#include <iQSM/_forwards.h>
#include <iQSM/field.h>
#include <iQSM/identifier.h>
#include <iQSM/meta/aspect_id.h>
#include <iQSM/meta/concepts.h>
#include <iQSM/references.h>
#include <iQSM/schema.h>

namespace iqsm {

    struct WorldInterface {
        using Id = Identifier<WorldInterface>;
        using TypeId = FieldAbstract::RuntimeTypeId;
        using Container = base::ImmutableUnorderedMap<FieldAbstract::RuntimeTypeId, cref<FieldAbstract>>;

        const Schema schema;
        const resources::Provider resources;

        virtual cref<FieldAbstract> field(TypeId rttid) const = 0;

        /// Same ownership as this handle; for `WorldObject` — concrete `World` without a cross-cast.
        virtual World share() const = 0;

        template<meta::Aspect Meta>
        Field<Meta> field() const;

        virtual ~WorldInterface() = default;

    protected:
        WorldInterface(Schema s, resources::Provider res)
            : schema(std::move(s))
            , resources(std::move(res))
        {}

        void basis_required(TypeId rttid) const {
            if (not schema->aspects.contains(rttid)) {
                throw std::runtime_error("WorldInterface: aspect is not in schema");
            }
        }
    };
}

//impl:
namespace iqsm {
    template<iqsm::meta::Aspect Meta>
    iqsm::Field<Meta> iqsm::WorldInterface::field() const {
        const TypeId rttid = types::aspectId<Meta>();
        return base::shared_ref_cast<const FieldData<Meta>>(field(rttid));
    }
}
