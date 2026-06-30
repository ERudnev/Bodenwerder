#include <fQSM/processing/contexts/operational.h>

#include <fQSM/logger.h>
#include <fQSM/utility/logging.h>

namespace fqsm::processing::context {

    Operational::Operational(const State& initial, PatchRef patch, Upstream cb)
        : accumulator(std::move(patch))
        , world(initial, accumulator, {})
        , callback(cb)
    {}

    void Operational::collapse() {
        if (logger::settings::processing::contextResult)
            utility::log_patch("context deleted, sending patch", fqsm::freeze(accumulator));
        if (callback)
            callback(accumulator);
        callback = nullptr;
    }

}