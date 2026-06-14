#pragma once

#include <memory>

#include <fQSM/references.h>
#include <fQSM/meta/concepts.h>
#include <fQSM/state/_forwards.h>

//#include <fQSM/state/world.h>

namespace fqsm::processing {

    using World = ::fqsm::state::world::Actual;
    using Patch = ::fqsm::state::world::Patch;

    struct Commit;
    struct Gate;
    struct Review;
    using ContextShared = std::shared_ptr<Commit>;
}

namespace fqsm {
    //using Reading = const processing::View&; // cleanup
    using Reading = const ::fqsm::state::world::Actual&;
    using Writing = processing::Gate;
    using Reviewing = processing::Review;
    using Immediate = ::fqsm::state::world::Actual&;
}