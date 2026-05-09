#pragma once

#include <initializer_list>

#include <iQSM/meta/concepts.h>
#include <iQSM/state/_forwards.h>
#include <iQSM/state/mechanism.h>
#include <iQSM/state/schema.h>

namespace iqsm::manipulator::schema {
    // for any Aspect Type, schema supports all versioning policies as own "mode"
    using Layer = state::policy::versioning;

    iqsm::Schema merge(std::initializer_list<iqsm::Schema> parts);

    template<meta::Aspect Meta, Layer versioning>
    iqsm::Schema aspect();
}

// impl:
namespace iqsm::manipulator::schema {
    template<meta::Aspect Meta, Layer versioning>
    iqsm::Schema aspect() {
        auto out = base::make_shared<iqsm::state::SchemaData>();
        iqsm::state::SchemaData::Aspect aspect;
        aspect.layer = versioning;
        out->aspects.emplace(iqsm::state::RAId{typeid(Meta)}, aspect);
        return iqsm::freeze(out);
    }
}