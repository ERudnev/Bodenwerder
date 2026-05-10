#include <iQSM/state/delta.h>

namespace iqsm::state {
    bool DeltaData::empty() const {
        return versioned.empty() and operational.empty();
    }
}
