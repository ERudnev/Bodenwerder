#pragma once

#include <base/maybe.h>

#include <rmmr/math.q1.h>
#include <rmmr/resources/shadows.q1.h>
#include <rmmr/scene/node.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::scene {

    using namespace fqsm::api;

    struct Light : Feature<Light, Node> {
        struct Quantum {
            RGB color;
            float intensity;
            float range;
            base::maybe<resource::shadow::Asset::Id> shadow;
        };
        struct Actions : BaseActions {
            static auto create(Writing, Locator, RGB, float intensity, float range) -> Id;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
