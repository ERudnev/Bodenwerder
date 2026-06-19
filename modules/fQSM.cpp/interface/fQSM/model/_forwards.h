#pragma once

#include <fQSM/references.h>

// model forwards
namespace fqsm::model::structure {
    struct AspectGraph;
}

namespace fqsm::model::complex {
    class State;
    class Reality;
    class Draft;
    struct Patch;
}

namespace fqsm::analysis {
    struct Patch;
}

namespace fqsm::model::linear {

    //this is base class, acting as forwarding for containers:
    namespace state { struct Erased { ~Erased()=default; }; }
    namespace patch { struct Erased { ~Erased()=default; }; }
    namespace draft { struct Erased { ~Erased()=default; }; }
    namespace preview { struct Erased { ~Erased()=default; }; }
}

// alias, mostly for external use
namespace fqsm {
    using Patch = ::fqsm::model::complex::Patch;
    using Schema = cref<model::structure::AspectGraph>;
    using State = model::complex::State;
    //using Reality = model::complex::Reality;
    //using WorldAddressable = model::complex::StateAddressable;
    // hiding as "1s class": using Patch = complex::Patch;
}
