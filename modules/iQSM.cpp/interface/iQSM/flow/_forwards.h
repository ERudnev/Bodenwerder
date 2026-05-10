#pragma once

#include <iQSM/references.h>

#include <iQSM/state/_forwards.h>

// forwaring of iqsm::state::View... here could be just include?
namespace iqsm::state { struct View; }
namespace iqsm::flow::internals { struct Permit; }

namespace iqsm {
    using Reading = cref<state::View>;
    using Writing = ::iqsm::flow::internals::Permit;
}