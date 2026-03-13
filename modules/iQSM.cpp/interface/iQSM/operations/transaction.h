#pragma once

#include <iQSM/world.h>
#include <iQSM/delta.h>

namespace iqsm::ops {   
    class Transaction {
    public:
        class Context {
        public:
            World& world;
            void absorb(Delta);
            void integrate();
            void validate();

        private:
            friend class Transaction;
            explicit Context(Transaction& owner);

            Transaction* owner;
        };

        Transaction(const World&) = delete;
        Transaction(const Transaction&) = delete;
        Transaction& operator=(const Transaction&) = delete;
        Transaction(Transaction&&) = delete;
        Transaction& operator=(Transaction&&) = delete;

        static Transaction accumulator(World world);
        static Transaction integrator(World world);
        operator Context();
        Context context();
        void absorb(Delta);
        void integrate();
        void validate();

        // returns full accumulated Delta (regardless of "world")
        Delta summary() const; // counts summary of all nested Deltas, use it once on Transaction is done

        World world; // used between nested transactions to make changes dependent

    private:
        explicit Transaction(World&& world, bool auto_integrate);

        bool auto_integrate;
        Delta inbox; // accumulates all absorbed deltas before integrate
        Delta integrated; // integrated but not validated with local world summary
    };

    using Context = Transaction::Context;
}


