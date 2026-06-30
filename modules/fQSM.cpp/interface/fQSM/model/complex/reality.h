#pragma once

#include <fQSM/model/complex/state.h>
#include <fQSM/model/linear/reality.h>
#include <fQSM/model/structure/composite.h>
#include <fQSM/model/linear/state.h>

namespace fqsm::model::complex {

    class Reality : public State {
    public:
        Reality(Schema schema) : State(schema) { initStructure(); }
        Reality(const State& source);

        // schema
        template<category::Any Meta>
        static ref<linear::state::Erased> clone(const State& source) {
            return linear::Reality<Meta>::from(source.aspect<Meta>());
        }

    protected:
        cref<Erased> getLine(Rtid typeId) const override { return lines.container.at(typeId); }
        ref<Erased> getLine(Rtid typeId) override { return lines.container.at(typeId); }
        const Composition& composition() const override { return lines; }
        Composition& composition() override { return lines; }

        void initStructure();

        Composite<linear::state::Erased> lines;
    };
}
