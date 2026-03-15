#pragma once

#include <iQSM/delta.h>
#include <iQSM/operations/integration.h>
#include <iQSM/repository/commit.h>
#include <iQSM/world.h>

namespace iqsm::repo {

    // Accumulates changes as a thick delta without integrating/validating into a World.
    // `current` is kept as an "sync" snapshot for workers that require a World to read from.
    class Sequence {
    public:
        Sequence(World starting) : current(starting) {}

        // Accumulate changes (no integrate/validate).
        void absorb(Delta delta) {
            current = ops::integrate(current, delta);
            accumulated = ops::merge(accumulated, std::move(delta));
        }

        // Current accumulated changes.
        Delta delta() const { return accumulated; }

        // Return accumulated delta and reset to empty (non-const).
        Delta push() {
            auto out = accumulated;
            accumulated = ::iqsm::delta::empty();
            return out;
        }

        // One-step conversion into a value-type handle compatible with experimental helpers.
        operator repo::Commit() {
            return repo::Commit{
                current,
                [this](Delta delta) { this->absorb(std::move(delta)); },
            };
        }

        operator iqsm::World() const { return current; }

    private:
        World current;
        Delta accumulated = ::iqsm::delta::empty();
    };
}

