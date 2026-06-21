#pragma once

#include <fQSM/references.h>
#include <fQSM/meta/categories.h>

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

namespace fqsm::model::linear {
    template<meta::category::Any T>
    class Reality;
}

namespace fqsm::analysis {
    struct Patch;
}

namespace fqsm::model::linear {

    //this is base class, acting as forwarding for containers:
    namespace state { struct Erased { virtual ~Erased()=default; }; }
    namespace patch { struct Erased { virtual ~Erased()=default; }; }
    namespace preview { struct Erased { virtual ~Erased()=default; }; }
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
