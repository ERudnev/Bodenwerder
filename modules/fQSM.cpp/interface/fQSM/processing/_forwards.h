#pragma once

#include <fQSM/references.h>
#include <fQSM/state/_forwards.h>

#include <fQSM/state/world.h>

namespace fqsm::processing {
    struct Context;
    struct Permit;
    using Channel = std::shared_ptr<Context>;
}

namespace fqsm::processing {
    using View = ::fqsm::state::world::View;
    using Data = ::fqsm::state::world::Data;
    using Patch = ::fqsm::state::world::Patch;    
}

namespace fqsm {
    using Reading = const state::world::View&; // cref?
    using Writing = processing::Permit;
}