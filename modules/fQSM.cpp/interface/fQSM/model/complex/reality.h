#pragma once

#include <fQSM/model/complex/state.h>
#include <fQSM/model/structure/composite.h>
#include <fQSM/model/linear/state.h>

namespace fqsm::model::complex {

    class Reality : public State {
    public:
        Reality(Schema schema) : State(schema) { generate(); }

    protected:
        cref<Erased> aspect(Rtid) const override;
        ref<Erased> aspect(Rtid) override;

        void generate();

        Composite<linear::state::Erased> lines;
    };
}
