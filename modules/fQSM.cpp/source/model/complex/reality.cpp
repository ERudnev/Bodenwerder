#include <fQSM/model/complex/reality.h>

namespace fqsm::model::complex {

    cref<State::Erased> Reality::aspect(Rtid typeId) const {
        _INCOMPLETE_;
        return lines.at(typeId);
    }

    ref<State::Erased> Reality::aspect(Rtid typeId) {
        _INCOMPLETE_;
        return lines.at(typeId);
    }

    void Reality::generate() {
        // TODO: iterate full Schema and make zero-lines
        _INCOMPLETE_;
    }
}