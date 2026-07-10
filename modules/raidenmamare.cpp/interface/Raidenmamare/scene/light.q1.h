#pragma once

#include <Raidenmamare/scene/node.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::scene {

    using namespace fqsm::api;

    struct Light : Feature<Light, Node> {
        struct Quantum {
            RGB color;
            float intensity;
            float range;
        };
        struct Actions : BaseActions {
            static auto create(Writing, Locator, RGB, float intensity, float range) -> Id;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
