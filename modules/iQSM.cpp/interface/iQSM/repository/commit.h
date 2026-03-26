#pragma once

#include <functional>
#include <utility>

#include <iQSM/_forwards.h>

namespace iqsm::repo {

    // Value-type commit handle:
    // - copyable/movable
    // - no virtual/polymorphism
    // - delegates `push()` into captured receiver (e.g. Branch)
    //
    // IMPORTANT USAGE NOTE
    // `Commit` is meant to be obtained from repository objects (e.g. `repo::Branch`, `repo::Sequence`, `repo::Accumulator`)
    // via implicit conversion. Do not construct it manually in user code.
    //
    // Rationale:
    // - repo objects define the write policy (when/how changes are applied and validated)
    // - converting from a repo object captures the correct receiver and a consistent snapshot (`initial`) for that call
    // - explicit construction tends to leak implementation details into user code and makes misuse (reusing stale snapshots) more likely
    struct Commit final {
        World initial;
        std::function<void(Delta)> receiver;

        Commit(World initial, std::function<void(Delta)> receiver)
            : initial(std::move(initial))
            , receiver(std::move(receiver))
        {}

        void push(Delta delta) const {
            receiver(std::move(delta));
        }
    };
}