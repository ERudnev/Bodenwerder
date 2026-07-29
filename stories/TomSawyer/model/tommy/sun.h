#pragma once

#include <tommy/gameObject.h>

#include <fQSM/api/interface.h>

namespace tommy {

    using namespace fqsm::api;

    struct Sun : Feature<Sun, GameObject> {
        static constexpr integer sprite_index = 261; // Kenney ufoYellow
        static constexpr float sprite_scale = 2.5f;
        static constexpr float hull_size = 3.0f;
        static constexpr integer max_hitpoints = 10000;
        static constexpr float pull = 0.0008f;
        struct Quantum {};
        struct Actions : BaseActions {
            static void attract(Writing);
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

}
