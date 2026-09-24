#pragma once

#include <optional>

#include <base/maybe.h>

#include <fQSM/meta/interface.include.h>
#include <fQSM/processing/contexts/session.h>

namespace fqsm::manipulation::temp_sugar {

    // Removes the referenced item (when set) and clears the reference.
    template<category::Any Meta>
    std::nullopt_t drop_reference(Writing context, base::maybe<Identifier<Meta>>& maybeId) {
        if (maybeId)
            meta::facade_t<Meta>::remove(context, *maybeId);
        maybeId.reset();
        return std::nullopt;
    }
}
