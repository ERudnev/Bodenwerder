#pragma once

#include <iQSM/flow/_forwards.h>
#include <iQSM/flow/internals/permit.h>

namespace iqsm {
    // IMPORTANT NOTICE:
    // if you considering to make flow::Channel as part of API - you are wrong!
    using Writing = ::iqsm::flow::internals::Permit;
}