#pragma once

#include <fQSM/model/_forwards.h>
#include <fQSM/processing/_forwards.h>

namespace fqsm::processing::algorithm {
    void merge(const model::complex::State& base, fqsm::ref<Patch> target, fqsm::cref<Patch> source);
}
