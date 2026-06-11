#pragma once

#include <memory>

#include <fQSM/references.h>
#include <fQSM/meta/concepts.h>
#include <fQSM/state/_forwards.h>

//#include <fQSM/state/world.h>

namespace fqsm::processing {
    using View = ::fqsm::state::world::View;
    using Data = ::fqsm::state::world::Data;
    using Patch = ::fqsm::state::world::Patch;

    struct Commit;
    struct Gate;
    template<meta::aspect::Any Meta>
    struct Immediate;
    struct Review;
    using ContextShared = std::shared_ptr<Commit>;
}

namespace fqsm {
    using Reading = const processing::View&;
    using Writing = processing::Gate;
    using Reviewing = processing::Review;

    template<meta::aspect::Any Meta>
    using Immediate = processing::Immediate<Meta>;
}