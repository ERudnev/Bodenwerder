#pragma once

#include <utility>

#include <iQSM/internals/fields_mutable.h>
#include <iQSM/meta/concepts.h>
#include <iQSM/meta/facade.h>
#include <iQSM/repository/commit.h>
#include <iQSM/repository/sequence.h>
#include <iQSM/world.h>

namespace iqsm::detail::lifecycle {
    template<meta::Aspect Meta>
    void pre_remove_action_into_accumulator(World world, internals::FieldsMutable& acc, Id<Meta> id, Item<Meta> before) {
        using Quantum = ::iqsm::Quantum<Meta>;

        if constexpr (requires { Meta::Operations::pre_remove_action(std::declval<repo::Commit>(), std::declval<Id<Meta>>(), std::declval<const Quantum&>()); }) {
            repo::Sequence staged{world};
            Meta::Operations::pre_remove_action(staged, id, *before);
            acc.absorb(world->schema, staged.push());
        }
    }
}
