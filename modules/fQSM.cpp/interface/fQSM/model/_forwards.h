#pragma once

#include <fQSM/references.h>

// model forwards
namespace fqsm::model::structure {
    struct AspectGraph;
}

namespace fqsm::model::complex {
    struct State;
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
namespace fqsm {
    using Patch = ::fqsm::model::complex::Patch;
    using Schema = cref<model::structure::AspectGraph>;
    using World = model::complex::State;
    // hiding as "1s class": using Patch = complex::Patch;
}
