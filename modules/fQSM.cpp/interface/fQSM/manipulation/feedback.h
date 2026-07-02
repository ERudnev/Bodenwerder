#pragma once

#include <format>
#include <string>

#include <fQSM/meta/interface.include.h>
#include <fQSM/meta/rtid.h>
#include <fQSM/processing/contexts/review.h>

namespace fqsm::manipulation::feedback {

    /*template<category::Any Meta>
    void critical(processing::Review context, std::string reason) {
        context.result.critical.push_back(
            std::format(R"(Aspect({}): {})", Rtid::name<Meta>(), reason));
    }*/

    inline void critical(processing::Review context, std::string reason) {
        context.result.critical.push_back(reason);
    };
}
