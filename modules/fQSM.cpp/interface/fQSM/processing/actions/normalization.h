#pragma once

#include <fQSM/model/_forwards.h>
#include <fQSM/processing/review.h>

namespace fqsm::processing::actions {

    //auto normalize(Reading base, const model::complex::Patch&) -> review::Notes;
    auto update(model::complex::Reality&, fqsm::cref<Patch>, Rtid::Set taintedLines) -> review::Notes;
}