#include <fQSM/processing/contexts/operational.h>

#include <fQSM/utility/logging.h>

namespace fqsm::processing::context {

    Operational::Operational(const State& initial, PatchRef patch, Upstream cb)
        : accumulator(std::move(patch))
        , world(initial, accumulator, {})
        , callback(cb)
    {}

    void Operational::collapse() {
        _DBG_TX_("context is up to close: patch={}", fqsm::utility::format_patch(fqsm::freeze(accumulator)));
        if (callback)
            callback(accumulator);
        callback = nullptr;
    }

}
