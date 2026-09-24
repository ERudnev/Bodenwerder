#pragma once

#include <cstddef>
#include <fQSM/references.h>
#include <fQSM/meta/categories.h>

// model forwards


namespace fqsm::model::elementary {
    // consider to use experimental "elementary" layer
}

namespace fqsm::model::complex {
    class State;
    class Reality;
    class Draft;
    struct Patch;
    class Future;
    class LinePool;
}

namespace fqsm::model::intertype {
    struct Graph;
}

namespace fqsm::erased {
    class ReadLine;
    class Line;
    class FutureLine;
}

namespace fqsm::model::linear {
    namespace state {
        // Owner handle for typed per-slot views cached inside complex states.
        struct Erased {
            virtual ~Erased() = default;
            // Points a pooled view at other lines of the same slot.
            virtual void rebind(const erased::ReadLine& reader, erased::Line* writable, erased::FutureLine* future) = 0;
        };
    }
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
