#pragma once

#include <fQSM/processing/_forwards.h>
#include <fQSM/model/linear/delta.h>
#include <fQSM/model/complex/patch.h>
#include <fQSM/model/complex/state.h>

namespace fqsm::processing::algorithm {
    void merge(const model::complex::State& base, fqsm::ref<Patch> target, fqsm::cref<Patch> source);
}

namespace fqsm::processing::algorithm::details {
    template<category::Any Meta>
    void merge(const model::complex::State& base, model::complex::Patch& target, const model::complex::Patch& source) {
        auto& targetPatch = target.aspect<Meta>();
        const auto& sourcePatch = source.aspect<Meta>();

        if (sourcePatch.global.has_value()) targetPatch.global = sourcePatch.global;

        const model::linear::Delta<Meta> delta{base.aspect<Meta>(), sourcePatch, model::linear::Delta<Meta>::Mode::clean};

        for (const auto entry : delta) {
            if (entry.add() || entry.update()) targetPatch.items.modify(entry.id, *entry.after);
            if (entry.remove()) targetPatch.items.insert(entry.id, std::nullopt);
        }
    }

    template<category::Any Meta>
    void merge(const model::complex::State& base, fqsm::ref<Patch> target, fqsm::cref<Patch> source) {
        merge<Meta>(base, *target, *source);
    }
}
