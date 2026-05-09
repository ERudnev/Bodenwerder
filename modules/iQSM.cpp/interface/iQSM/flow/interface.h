#pragma once

#include <iQSM/references.h>
#include <iQSM/flow/permit.h>
#include <iQSM/flow/reading.h>

namespace iqsm {
    namespace state { struct View; }
    using Writing = ::iqsm::flow::Permit;
    // Reading is added to ::iqsm root by "reading.h"

    // IMPORTANT NOTICE:
    // if you considering to make flow::Channel as part of API - you are wrong!
}