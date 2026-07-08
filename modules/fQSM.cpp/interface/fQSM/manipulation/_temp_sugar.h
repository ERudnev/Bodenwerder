#pragma once

#include <base/maybe.h>

#include <fQSM/meta/interface.include.h>
#include <fQSM/meta/rtid.h>
#include <fQSM/processing/contexts/operational.h>

namespace fqsm::manipulation::temp_sugar {

    template<category::Any Meta>
    std::nullopt_t drop_reference(Writing context, base::maybe<Identifier<Meta>>& maybeId) {
        if (maybeId)
            Meta::Actions::remove(context, maybeId);
        maybeId.reset();
        return std::nullopt;
    }
}