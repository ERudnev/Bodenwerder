#include <fQSM/model/complex/state.h>

namespace fqsm::model::complex {

    cref<State::Erased> Reality::aspect(composite::TypeId typeId) const {
        _INCOMPLETE_;
        return lines.at(typeId);
    }

    ref<State::Erased> Reality::aspect(composite::TypeId typeId) {
        _INCOMPLETE_;
        return lines.at(typeId);
    }

    void Reality::generate() {
        // TODO: iterate full Schema and make zero-lines
        _INCOMPLETE_;
    }
}