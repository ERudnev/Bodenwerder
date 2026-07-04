#pragma once

#include <cstddef>
#include <fQSM/references.h>
#include <fQSM/meta/categories.h>

// model forwards


namespace fqsm::model::elementary {
    // consider to use experimental "elementary" layer
}

namespace fqsm::model::linear {
    template<meta::category::Any T>
    class Reality;
}

namespace fqsm::model::complex {
    class State;
    class Reality;
    class Draft;
    struct Patch;
}

namespace fqsm::model::intertype {
    struct Graph;
}

// suspecious (cleanup)
namespace fqsm::model::linear {

    //this is base class, acting as forwarding for containers:

    // TODO: keep this as forwards, move daclaration to linear/state.h
    namespace state {
        struct Erased {
            virtual ~Erased()=default;
            virtual std::size_t quanta() const = 0;
        };
    }
    namespace patch {
        struct Erased {
            virtual ~Erased()=default;
            virtual bool has_changes() const = 0;
        };
    }
    namespace preview { struct Erased { virtual ~Erased()=default; }; }
}

// alias, mostly for external use
namespace fqsm {
    using Patch = ::fqsm::model::complex::Patch;
    using Schema = cref<model::intertype::Graph>;
    using State = model::complex::State;
    //using Reality = model::complex::Reality;
    //using WorldAddressable = model::complex::StateAddressable;
    // hiding as "1s class": using Patch = complex::Patch;
}
