#pragma once

#include <iQSM/delta.h>
#include <iQSM/internals/fields_mutable.h>
#include <iQSM/operations/integration.h>
#include <iQSM/repository/commit.h>
#include <iQSM/world.h>

namespace iqsm::repo {

    // Accumulates independent changes as a thick delta without integrating/validating into a World.
    // `head` is a read-only snapshot for workers that require a World to read from.
    class Accumulator {
    public:
        Accumulator(World starting) : head(starting) {}

        // Accumulate changes (no integrate/validate, no head movement).
        void absorb(Delta delta) {
            accumulated.absorb(head->schema, std::move(delta));
        }

        // Current accumulated changes.
        Delta delta() const { return accumulated.snapshot(head->schema); }

        // Return accumulated delta and reset to empty (non-const).
        Delta push() {
            return accumulated.push();
        }

        // One-step conversion into a value-type handle compatible with experimental helpers.
        operator repo::Commit() {
            return repo::Commit{
                head,
                [this](Delta delta) { this->absorb(std::move(delta)); },
            };
        }
        operator iqsm::World() const { return head; }

    private:
        const World head;
        internals::FieldsMutable accumulated{};
    };
}

