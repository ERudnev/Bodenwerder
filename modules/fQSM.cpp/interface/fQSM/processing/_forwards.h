#pragma once

#include <memory>

#include <fQSM/references.h>
#include <fQSM/state/_forwards.h>

//#include <fQSM/state/world.h>

namespace fqsm::processing {
    using View = ::fqsm::state::world::View;
    using Data = ::fqsm::state::world::Data;
    using Patch = ::fqsm::state::world::Patch;    

    struct Context;
    struct Gate;
    using ContextShared = std::shared_ptr<Context>;
}

namespace fqsm {
    using Reading = const processing::View&;
    using Writing = processing::Gate;
}