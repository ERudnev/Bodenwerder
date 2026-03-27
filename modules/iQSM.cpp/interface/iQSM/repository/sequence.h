#pragma once

#include <iQSM/delta.h>
#include <iQSM/internals/fields_mutable.h>
#include <iQSM/operations/integration.h>
#include <iQSM/repository/commit.h>
#include <iQSM/world.h>

namespace iqsm::repo {

    // Accumulates changes as a thick delta.
    // Also keeps an up-to-date `current` world snapshot by integrating each absorbed delta,
    // so subsequent operations can depend on previous writes.
    class Sequence {
    public:
        Sequence(World starting) : current(starting) {}

        // Accumulate changes (integrates into `current`, no validation).
        void absorb(Delta delta) {
            current = operations::integrate(current, delta);
            accumulated.absorb(current->schema, std::move(delta));
        }

        // Current accumulated changes.
        Delta delta() const { return accumulated.snapshot(current->schema); }

        // Return accumulated delta and reset accumulator to empty (non-const).
        Delta push() {
            return accumulated.push();
        }

        // One-step conversion into a value-type handle compatible with experimental helpers.
        operator repo::Commit() {
            return repo::Commit{
                current,
                [this](Delta delta) { this->absorb(std::move(delta)); },
            };
        }

    private:
        World current;
        internals::FieldsMutable accumulated{};
    };
}

