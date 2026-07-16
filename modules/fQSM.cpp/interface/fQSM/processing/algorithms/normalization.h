#pragma once

#include <fQSM/model/_forwards.h>
#include <fQSM/processing/contexts/review.h>

namespace fqsm::processing::algorithm {

    //auto normalize(Reading base, const model::complex::Patch&) -> model::complex::Patch::Result;
    auto update(model::complex::Reality&, fqsm::ref<Patch>, Rtid::Set taintedLines) -> model::complex::Patch::Result;
}