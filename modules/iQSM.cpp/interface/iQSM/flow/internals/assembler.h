#pragma once

#include <utility>

#include <iQSM/state/_forwards.h>
#include <iQSM/state/delta.h>

namespace iqsm::flow::internals {
    struct Assembler {
        ref<state::DeltaData> accumulator;

        Assembler();
        explicit Assembler(ref<state::DeltaData> delta);

        Delta delta() const;
        Delta push();

        void absorb(Schema schema, Delta delta);

    };
}

