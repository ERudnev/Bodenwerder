#pragma once

#include <fQSM/references.h>

// model forwards
namespace fqsm::model::structure {
    struct AspectGraph;
}

namespace fqsm::model::complex {
    struct State;
    struct StateAddressable;
    struct Patch;
    struct Future;
    struct Draft;
}

namespace fqsm::analysis {
    struct Patch;
}

namespace fqsm::model::linear {

    //this is base class, acting as forwarding for containers:
    namespace state { struct Erased { ~Erased()=default; }; }
    namespace patch { struct Erased { ~Erased()=default; }; }
    namespace preview { struct Erased { ~Erased()=default; }; }
}

// alias, mostly for external use
namespace fqsm::model {
    using Schema = cref<structure::AspectGraph>;
    using World = complex::State;
    using WorldAddressable = complex::StateAddressable;
    // hiding as "1s class": using Patch = complex::Patch;
}
