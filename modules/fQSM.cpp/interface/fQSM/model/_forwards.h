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
    // The lines one typed view reads and writes. Typed views add no state to this struct,
    // so a complex state keeps the views of all aspects in one untyped array (State::slot).
    struct Lines {
        const erased::ReadLine* reader = nullptr;
        erased::Line* writable = nullptr;       // the reader itself, when in-place access is allowed (Realm lines)
        erased::FutureLine* future = nullptr;   // the reader itself, when writes go into a patch
    };
}

namespace fqsm {
    using Patch = ::fqsm::model::complex::Patch;
    using Schema = cref<model::intertype::Graph>;
    using State = model::complex::State;
}
