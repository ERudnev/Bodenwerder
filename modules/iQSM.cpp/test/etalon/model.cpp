#include "model.h"

#include <algorithm>

namespace iqsm_internal_model {

    using namespace iqsm::dsl_gateway;

    auto Basic::Operations::total_value(Reading world) -> integer {
        integer total = 0;
        for (const auto& kv : world->field<Basic>()->container) {
            total += kv.second->value;
        }
        return total;
    }

    struct Basic_private : Basic::Operations {
        static ItemChange clamp_value(Reading, Id, const Quantum& original) {
            auto updated = original;
            if (updated.value < integer{0}) updated.value = integer{0};
            if (updated.value > integer{100}) updated.value = integer{100};
            if (updated.value == original.value) return {};
            return updated;
        }

        static auto min_max_total(Reading world) -> std::pair<integer, integer> {
            const auto& c = world->field<Basic>()->container;
            const auto it0 = c.begin();
            if (it0 == c.end()) return {integer{0}, integer{0}};

            integer min = it0->second->value;
            integer max = it0->second->value;
            for (auto it = std::next(it0); it != c.end(); ++it) {
                const auto v = it->second->value;
                min = std::min(min, v);
                max = std::max(max, v);
            }
            return {min, max};
        }
    };

    auto Basic::Operations::normalized(Reading world, Id id) -> float {
        const auto [min, max] = Basic_private::min_max_total(world);
        if (min == max) return 0.0f;

        const auto& q = ops::particle::get<Basic>(world, id);
        const auto num = static_cast<float>(q.value - min);
        const auto den = static_cast<float>(max - min);
        return num / den;
    }

    const Invariants Basic::invariants{
        .structural = {},
        .logical = {
            invariant::for_each_item<Basic, &Basic_private::clamp_value>,
        },
    };

    const Invariants Essentials::invariants{
        .structural = {
            invariant::anchor_attribute<Basic, Essentials>,
        },
        .logical = {},
    };

    const Invariants Optionals::invariants{
        .structural = {
            invariant::anchor_attribute<Basic, Optionals>,
        },
        .logical = {},
    };
}