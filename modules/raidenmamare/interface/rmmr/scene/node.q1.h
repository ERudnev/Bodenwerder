#pragma once

#include <rmmr/math.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::scene {

    using namespace fqsm::api;

    struct Node : Entity<Node> {
        struct Quantum {
            Pose pose;
            bool visible;
        };
        struct Actions : BaseActions {
            static auto transform(Reading, Id) -> mat4;
            static void setVisible(Writing, Id, bool);
            static auto create(Writing, Pose) -> Id;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
