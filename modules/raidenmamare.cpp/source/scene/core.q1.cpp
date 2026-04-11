#include <Raidenmamare/scene/core.q1.h>

#include <iQSM/api/_gateway_equal.h>

namespace rmmr::scene {
    struct Core_private : Core::Operations {
        static auto cameras_registered(Reading world, Id, const Quantum& before) -> ItemChange {
            auto after = before;
            after.cameras.clear();
            after.cameras.reserve(before.nodes.size());

            for (const auto node : before.nodes) {
                if (ops::particle::exists<Camera>(world, node)) {
                    after.cameras.push_back(node);
                }
            }

            if (ops::particle::equal<Core>(after, before)) return {};
            return after;
        }

        static auto lights_registered(Reading world, Id, const Quantum& before) -> ItemChange {
            auto after = before;
            after.lights.clear();
            after.lights.reserve(before.nodes.size());

            for (const auto node : before.nodes) {
                if (ops::particle::exists<Light>(world, node)) {
                    after.lights.push_back(node);
                }
            }

            if (ops::particle::equal<Core>(after, before)) return {};
            return after;
        }
    };

    const Invariants Core::invariants{
        .structural = {},
        .logical = {
            invariant::for_each_item<Core, &Core_private::cameras_registered>,
            invariant::for_each_item<Core, &Core_private::lights_registered>,
        },
    };
}
