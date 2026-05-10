#include <iQSM/state/delta.h>

namespace iqsm::state {
    bool DeltaData::empty() const {
        return slices.empty();
    }
}
