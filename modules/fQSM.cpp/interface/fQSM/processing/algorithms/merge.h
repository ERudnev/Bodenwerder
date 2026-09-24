#pragma once

#include <fQSM/processing/_forwards.h>
#include <fQSM/erased/algorithms.h>
#include <fQSM/model/complex/patch.h>
#include <fQSM/model/complex/state.h>

namespace fqsm::processing::algorithm {
    void merge(const model::complex::State& base, fqsm::ref<Patch> target, fqsm::cref<Patch> source);
}

namespace fqsm::processing::algorithm::details {
    template<category::Any Meta>
    void merge(const model::complex::State& base, model::complex::Patch& target, const model::complex::Patch& source) {
        erased::merge_into(base.aspect<Meta>().line(), target.aspect<Meta>().line, source.aspect<Meta>().line);
    }

    template<category::Any Meta>
    void merge(const model::complex::State& base, fqsm::ref<Patch> target, fqsm::cref<Patch> source) {
        merge<Meta>(base, *target, *source);
    }
}
