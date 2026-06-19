#pragma once

#include <memory>

#include <fQSM/references.h>
#include <fQSM/meta/concepts.h>
#include <fQSM/model/_forwards.h>

//#include <fQSM/state/world.h>

namespace fqsm::processing {
    //using Patch = ::fqsm::model::Patch;
    struct Context;
    struct GateOperational;
    struct GateDirect;
    struct Review;
}

// exportin this as 1st class citizen of fQSM:
namespace fqsm {
    //using Reading = const processing::View&; // cleanup
    using Reading = const ::fqsm::model::complex::State&;
    using Writing = processing::GateOperational; // rename to "Draft"?
    using Access = processing::GateDirect;
    using Reviewing = processing::Review;
}