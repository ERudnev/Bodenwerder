#pragma once

#include <memory>

#include <iQSM/_forwards.h>
#include <iQSM/field.h>
#include <iQSM/meta/aspect_id.h>
#include <iQSM/references.h>
#include <iQSM/types.h>
#include <iQSM/collections/world.h>

namespace iqsm {
    struct WorldObject : WorldInterface, std::enable_shared_from_this<WorldObject> {
        const Id id;
        Container fields;

        explicit WorldObject(Schema s, resources::Provider res)
            : WorldInterface(std::move(s), std::move(res))
            , id(Id::generate_random())
        {}

        auto clone() const -> ref<WorldObject> {
            auto w = base::make_shared<WorldObject>(schema, resources);
            w->fields = fields;
            return ref<WorldObject>(std::move(w));
        }

        World share() const override;

        cref<FieldAbstract> field(TypeId rttid) const override;

        template<meta::Aspect Meta>
        Field<Meta> field() const;
    };
}

inline iqsm::World iqsm::WorldObject::share() const {
    return World(shared_from_this());
}

inline iqsm::cref<iqsm::FieldAbstract> iqsm::WorldObject::field(TypeId rttid) const {
    WorldInterface::basis_required(rttid);

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
