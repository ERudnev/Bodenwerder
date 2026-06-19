#pragma once

#include <fQSM/processing/_forwards.h>
#include <fQSM/model/linear/delta.h>

namespace fqsm::processing::actions {
    void merge(Reading base, fqsm::ref<Patch> target, fqsm::cref<Patch> source);
}

namespace fqsm::processing::actions::details {
    template<aspect::Any Meta>
    void merge(Reading base, model::complex::Patch& target, const model::complex::Patch& source) {
        auto& targetPatch = target.aspect<Meta>();
        const auto& sourcePatch = source.aspect<Meta>();

        if (sourcePatch.global.has_value()) targetPatch.global = sourcePatch.global;

        const model::linear::Delta<Meta> delta{base.aspect<Meta>(), sourcePatch, model::linear::Delta<Meta>::Mode::clean};

        for (const auto entry : delta.get()) {
            if (entry.add() || entry.update()) targetPatch.items.modify(entry.key, *entry.after);
            if (entry.remove()) targetPatch.items.insert(entry.key, std::nullopt);
        }
    }

    template<aspect::Any Meta>
    void merge(Reading base, fqsm::ref<Patch> target, fqsm::cref<Patch> source) {
        merge<Meta>(base, *target, *source);
    }
}
