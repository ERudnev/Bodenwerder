#pragma once

#include <iQSM/references.h>

// forwaring of iqsm::state::View... here could be just include?
namespace iqsm::state { struct View; }

namespace iqsm {
    using Reading = cref<state::View>;
}