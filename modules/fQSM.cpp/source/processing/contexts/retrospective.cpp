#include <fQSM/processing/contexts/retrospective.h>

#include <fQSM/utility/logging.h>

namespace fqsm::processing::context {

    Retrospective::Retrospective(const State& initial, PatchRef patch, Upstream cb)
        : accumulator(std::move(patch))
        , base(initial)
        , callback(cb)
    {}

    void Retrospective::collapse() {
        _DBG_TX_("retrospective is up to close: patch={}", fqsm::utility::format_patch(fqsm::freeze(accumulator)));
        if (callback)
            callback(accumulator);
        callback = nullptr;
    }

}
