#pragma once

#include <format>
#include <string>

#include <fQSM/meta/interface.include.h>
#include <fQSM/meta/rtid.h>
#include <fQSM/processing/contexts/review.h>

namespace fqsm::manipulation::feedback {

    // placeholder-state interface for Reactions
    inline void critical(processing::Review context, std::string reason) {
        context.result.critical.push_back(reason);
    };
}
