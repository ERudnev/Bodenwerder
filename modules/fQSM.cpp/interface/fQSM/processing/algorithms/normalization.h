#pragma once

#include <fQSM/model/_forwards.h>
#include <fQSM/processing/contexts/session.h>

namespace fqsm::processing::algorithm {

    // Normalizes the patch against the reality (waves of rules and reactions), then integrates it when no refusal was recorded.
    auto update(model::complex::Reality&, fqsm::ref<Patch>, Rtid::Set taintedLines) -> model::complex::Patch::Summary;
}