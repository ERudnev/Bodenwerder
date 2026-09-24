#pragma once

#include <format>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>

#include <base/logging.h>
#include <base/serialization.h>
#include <fQSM/meta/interface.include.h>
#include <fQSM/model/complex/patch.h>
#include <fQSM/processing/contexts/review.h>

namespace fqsm::utility {

    auto format_patch(cref<model::complex::Patch> patch) -> std::string;
    auto format_patch(const model::complex::Patch& patch) -> std::string;
    void log_patch(std::string_view legend, cref<model::complex::Patch> patch);
    void log_rejected_transaction(const model::complex::Patch::Summary&);

}
