#pragma once

#include <iQSM/_forwards.h>
#include <iQSM/internals/delta_builders.h>
#include <iQSM/meta/concepts.h>
#include <iQSM/meta/facade.h>
#include <iQSM/repository/commit.h>

namespace iqsm::helpers::binding {
    // Binding instances live in World like other particles.
    // Use `helpers::particle::{item,get,exists,...}` out of the box.
    template<meta::Binding Meta>
    auto declare(repo::Commit commit, ::iqsm::Quantum<Meta> quantum) -> Id<Meta>;
}

//impl:
namespace iqsm::helpers::binding {
    template<meta::Binding Meta>
    auto declare(repo::Commit commit, ::iqsm::Quantum<Meta> quantum) -> Id<Meta> {
        const auto id = Id<Meta>::generate_random();

        commit.push(internals::delta::make_atomic<Meta>(
            id,
            std::nullopt,
            base::make_shared<const ::iqsm::Quantum<Meta>>(std::move(quantum))));
        return id;
    }
}
