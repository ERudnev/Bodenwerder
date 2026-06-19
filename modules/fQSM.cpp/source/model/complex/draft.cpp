#include <fQSM/model/complex/draft.h>
#include <fQSM/model/structure/schema.h>

namespace fqsm::model::complex {
    // TODO: return imple here

    // draft tries to look like a State.
    cref<State::Erased> Draft::aspect(Rtid typeId) const {
        //return schema->nodes.at(typeId).binding.createPreview(state, patch);
    }

    ref<State::Erased> Draft::aspect(Rtid typeId) {
        //return schema->nodes.at(typeId).binding.createPreview(state, patch);
    }
}