#include <fQSM/processing/contexts/operational.h>

#include <fQSM/utility/logging.h>

namespace fqsm::processing::context {

    Operational::Operational(const State& initial, PatchRef patch, Upstream cb)
        : future(initial, patch, {})
        , callback(cb)
    {}

    void Operational::collapse() {
        _DBG_TX_("context is up to close: patch={}", fqsm::utility::format_patch(fqsm::freeze(future.patch())));
        if (callback)
            callback(future.patch());
        callback = nullptr;
    }

}
