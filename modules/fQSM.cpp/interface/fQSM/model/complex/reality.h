#pragma once

#include <fQSM/model/complex/state.h>
#include <fQSM/model/structure/composite.h>
#include <fQSM/model/linear/state.h>

namespace fqsm::model::complex {

    class Reality : public State {
    public:
        Reality(Schema schema) : State(schema) { initStructure(); }
        Reality(const State& source);

    protected:
        cref<Erased> getLine(Rtid typeId) const override { return lines.container.at(typeId); }
        ref<Erased> getLine(Rtid typeId) override { return lines.container.at(typeId); }
        const Composition& composition() const override { return lines; }
        Composition& composition() override { return lines; }

        void initStructure();

        Composite<linear::state::Erased> lines;
    };
}
