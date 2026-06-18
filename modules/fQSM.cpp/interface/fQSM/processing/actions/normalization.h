#pragma once

#include <fQSM/model/_forwards.h>
#include <fQSM/processing/review.h>

namespace fqsm::processing::actions {

    struct NormalizationResult {
        fqsm::ref<model::complex::Patch> patch;
        Review::Notes notes;
    };

    auto normalize(Reading base, const model::complex::Patch&) -> NormalizationResult;
    auto update(model::complex::StateAddressable&, const model::complex::Patch&, ) -> Review::Notes;
}