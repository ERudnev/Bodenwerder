#pragma once

#include <iQSM/world.h>
#include <iQSM/delta.h>

namespace iqsm::ops {
    class Transaction {
    public:
        enum class Policy {
            accumulator,
            integrator,
            validator,
        };

        Transaction(const World&) = delete;

        explicit Transaction(World&& initial, Policy policy);

        // helper-builders
        static Transaction accumulator(World initial);
        static Transaction integrator(World initial);
        static Transaction validator(World initial);

        // interceptors (preserve summary/dirty, override current and policy)
        static Transaction accumulator(World initial, Transaction&& from);
        static Transaction integrator(World initial, Transaction&& from);
        static Transaction validator(World initial, Transaction&& from);

        World current;
        Delta summary;

        Policy policy;

        // True iff `current` does not reflect accumulated `summary` (accumulator mode only).
        bool dirty = false;

        void absorb(Delta update);

        // Apply the whole summary in one go (useful for accumulator experiments).
        void flush();

        // Validate current world state (flushes first if needed).
        void validate();

    private:
        World apply(World world, Delta update) const;
    };
}


