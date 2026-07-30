#pragma once

#include <rmmr/scene/node.q1.h>

#include <fQSM/api/interface.h>

namespace kubes::phys {

    using namespace fqsm::api;

    struct Atom : Entity<Atom> {
        struct Quantum {
            vec3 current{};
            vec3 prev{};
            float mass = 1.0f;
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Visual : Entity<Visual> {
        struct Quantum {
            Affected<Atom> atom;
            Custody<rmmr::scene::Node> actor;
        };
        struct Actions : BaseActions {
            static void update(Writing);
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

}
