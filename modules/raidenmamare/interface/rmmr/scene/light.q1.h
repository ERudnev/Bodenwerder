#pragma once

#include <rmmr/math.q1.h>
#include <rmmr/scene/node.q1.h>

#include <cstdint>

#include <fQSM/api/interface.h>

namespace rmmr::scene {

    using namespace fqsm::api;

    struct Light : Feature<Light, Node> {
        enum class Kind : std::uint8_t {
            point,
            directional,
        };
        struct Quantum {
            Kind kind;
            RGB color;
            float intensity;
            float range;
        };
        struct Actions : BaseActions {
            static auto create(Writing, Pose, Kind, RGB, float intensity, float range) -> Id;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
