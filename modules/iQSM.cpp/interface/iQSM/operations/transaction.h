#pragma once

#include <utility>

#include <iQSM/world.h>
#include <iQSM/delta.h>
#include <iQSM/operations/integration.h>

namespace iqsm::ops {
    struct Transaction {
        Transaction(const World&) = delete;

        explicit Transaction(World&& initial_)
            : current(std::move(initial_))
            , summary(nullptr)
        {}

        World current;
        Delta summary;

        void absorb(Delta update) {
            current = integrate_raw(current, update);
            summary = merge(summary, update);
        }
    };
}


