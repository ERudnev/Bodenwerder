#pragma once

#include <iQSM/world.h>
#include <iQSM/delta.h>
#include <iQSM/operations/integration.h>

namespace iqsm {
    struct Transaction {
        explicit Transaction(World initial_)
            : current(initial_)
            , summary(nullptr)
        {}

        World current;
        Delta summary;

        World validated_world() const { return validate(current); } // validation is currently a no-op

        void absorb(Delta update) {
            current = integrate(current, update);
            summary = merge(summary, update);
        }
    };
}


