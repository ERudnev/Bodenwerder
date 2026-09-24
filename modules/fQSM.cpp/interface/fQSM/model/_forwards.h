#pragma once

#include <cstddef>
#include <fQSM/references.h>
#include <fQSM/meta/categories.h>

namespace fqsm::model::complex {
    class State;
    class Reality;
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

namespace fqsm::view {
    // Owner handle of the typed per-slot views that complex states cache (and the Realm pool keeps).
    struct SlotBase {
        virtual ~SlotBase() = default;
        // Points a pooled view at other lines of the same slot.
        virtual void rebind(const erased::ReadLine& reader, erased::Line* writable, erased::FutureLine* future) = 0;
    };
}

namespace fqsm {
    using Patch = ::fqsm::model::complex::Patch;
    using Schema = cref<model::intertype::Graph>;
    using State = model::complex::State;
}
