#pragma once

#include <fQSM/processing/_forwards.h>
#include <fQSM/state/delta.h>
#include <fQSM/state/patch.h>

namespace fqsm::processing::actions {
    void merge(Reading base, fqsm::ref<Patch> target, fqsm::cref<Patch> source);
}

// implementation/details
namespace fqsm::processing::actions::details {
    template<aspect::Any Meta>
    void merge(Reading base, fqsm::ref<Patch> target, fqsm::cref<Patch> source) {
        if (source->template global<Meta>().has_value()) {
            target->template global<Meta>() = source->template global<Meta>();
        }

        auto& targetItems = target->template items<Meta>();
        const state::slice::Delta<Meta> delta{
            base.template slice<Meta>(),
            source->template slice<Meta>()
        };

        for (const auto entry : delta) {
            if (entry.add() || entry.update()) {
                targetItems.insert(entry.id, *entry.after);
            }

            if (entry.remove()) {
                targetItems.insert(entry.id, std::nullopt);
            }
        }
    }
}