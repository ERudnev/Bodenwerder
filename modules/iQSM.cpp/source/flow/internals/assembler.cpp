#include <iQSM/flow/internals/assembler.h>
#include <iQSM/state/delta.h>
#include <iQSM/state/schema.h>

namespace iqsm::flow::internals {
    Assembler::Assembler() : accumulator(base::make_shared<state::DeltaData>())
    {}

    auto Assembler::delta() const -> Delta {
        return iqsm::freeze(accumulator);
    }

    auto Assembler::push() -> Delta {
        auto out = iqsm::freeze(accumulator);
        accumulator.kill();
        return out;
    }

    void Assembler::absorb(Schema schema, Delta delta) {

    }
}