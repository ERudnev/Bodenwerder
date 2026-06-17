#pragma once

#include <memory>

#include <fQSM/references.h>
#include <fQSM/meta/concepts.h>
#include <fQSM/model/_forwards.h>

//#include <fQSM/state/world.h>

namespace fqsm::processing {

    using World = ::fqsm::model::World;
    using Patch = ::fqsm::model::Patch;

    struct Commit;
    struct GateWriting;
    struct GateImmediate;
    struct Review;
    struct Immediate; // clarify this
    using ContextShared = std::shared_ptr<Commit>;
}

namespace fqsm {
    //using Reading = const processing::View&; // cleanup
    using Reading = const ::fqsm::state::world::Actual&;
    using Writing = processing::GateWriting;
    using Immediate = processing::GateImmediate;
    using Reviewing = processing::Review;
}