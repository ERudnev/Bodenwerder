#pragma once

#include <rmmr/math.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::scene {

    using namespace fqsm::api;

    struct Node : Entity<Node> {
        struct Quantum {
            Pos position;
            quat rotation;
        };
        struct Actions : BaseActions {
            static auto transform(Reading, Id) -> mat4;
            static auto hpb(Reading, Id) -> HPB;
            static void hpb(Writing, Id, HPB);
            static auto create(Writing, Locator) -> Id;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
