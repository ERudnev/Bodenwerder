#pragma once

#include <base/maybe.h>

#include <cstddef>

#include <iQSM/meta.h>
#include <iQSM/repository/commit.h>
#include <iQSM/world.h>

namespace tests::debug {

    template<iqsm::meta::Aspect Meta>
    auto read_world(iqsm::World world, iqsm::Id<Meta> id) -> base::maybe<iqsm::Quantum<Meta>> {
        if (not world->schema->aspects.contains(iqsm::Facet<Meta>::typeId)) return {};
        const auto field = world->field<Meta>();
        if (not field->container.contains(id)) return {};
        return *field->container.at(id);
    }

    template<iqsm::meta::Aspect Meta>
    auto read(iqsm::repo::Commit commit, iqsm::Id<Meta> id) -> base::maybe<iqsm::Quantum<Meta>> {
        return read_world<Meta>(commit.initial, id);
    }

    template<iqsm::meta::Aspect Meta>
    auto count_world(iqsm::World world) -> std::size_t {
        if (not world->schema->aspects.contains(iqsm::Facet<Meta>::typeId)) return 0;
        return world->field<Meta>()->container.size();
    }

    template<iqsm::meta::Aspect Meta>
    auto count(iqsm::repo::Commit commit) -> std::size_t {
        return count_world<Meta>(commit.initial);
    }
}