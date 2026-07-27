#include <fQSM/processing/contexts/retrospective.h>

namespace fqsm::processing::context {

    Retrospective::Retrospective(const State& initial, Operational::Ptr writer)
        : writer(std::move(writer))
        , base(initial)
    {}

}
