#pragma once

#include <fQSM/model/complex/state.h>

namespace fqsm::model::complex {

    class Reality : public State {
        Reality(Schema schema) : State(schema), { generate(); }

    protected:
        cref<Erased> aspect(meta::aspect::Rtid) const override;
        ref<Erased> aspect(meta::aspect::Rtid) override;

        void generate();

        Container lines;
    };
}
