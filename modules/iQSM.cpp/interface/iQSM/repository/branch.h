#pragma once

#include <iQSM/delta.h>
#include <iQSM/operations/integration.h>
#include <iQSM/repository/commit.h>
#include <iQSM/world.h>

namespace iqsm::repo {

    class Branch {
    public:
        //
        Branch(World starting) : current(starting) {}

        // branch ops:
        void rebase(World world) {
            if (world == current) return;
            current = ops::validate(std::move(world));
        }

        void absorb(Delta delta) {
            rebase(ops::integrate(current, std::move(delta)));
        }

        // One-step conversion into a value-type handles compatible with other iQSM parts
        operator repo::Commit() { return repo::Commit{current, [this](Delta delta) { this->absorb(std::move(delta)); },}; }
        operator iqsm::World() const { return current; }

    private:
        World current;
    };
}