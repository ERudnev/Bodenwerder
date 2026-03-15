#pragma once

#include <base/maybe.h>

#include <iQSM/meta.h>
#include <iQSM/world.h>

namespace tests::debug {

    template<iqsm::meta::Aspect Meta>
    auto read(iqsm::World world, iqsm::Id<Meta> id) -> base::maybe<iqsm::Quantum<Meta>> {
        if (not world->schema->aspects.contains(iqsm::Facet<Meta>::typeId)) return {};
        const auto field = world->field<Meta>();
        if (not field->container.contains(id)) return {};
        return *field->container.at(id);
    }
}