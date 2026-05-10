#include <iQSM/flow/processing.h>

#include <iQSM/state/delta.h>
#include <iQSM/state/world.h>

namespace iqsm::flow {

    World integrate(Reading source, Delta changes) {
        _INCOMPLETE_;
        return source->share();
    }

    void validateFull(Writing) {
        _INCOMPLETE_;
    }

    void validateSmart(Writing updated, Reading lastValidState) {
        _INCOMPLETE_;
    }
    Delta makeDelta(Reading from, Reading to) {
        _INCOMPLETE_;
        return iqsm::freeze(base::make_shared<state::DeltaData>());
    }

}