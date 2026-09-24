#pragma once

#include <string>

#include <base/logging.h>
#include <fQSM/model/complex/patch.h>

namespace fqsm::utility {

    auto format_patch(cref<model::complex::Patch> patch) -> std::string;
    auto format_patch(const model::complex::Patch& patch) -> std::string;
    void log_rejected_transaction(const model::complex::Patch::Summary&);

}
