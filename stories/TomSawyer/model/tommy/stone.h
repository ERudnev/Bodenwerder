#pragma once

#include <tommy/gameObject.h>

#include <fQSM/api/interface.h>

namespace tommy {

    using namespace fqsm::api;

    struct Stone : Feature<Stone, GameObject> {
        static constexpr integer sprite_index = 163; // Kenney meteorGrey_big1
        static constexpr float sprite_scale = 0.5f;
        struct Quantum {};
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

}
