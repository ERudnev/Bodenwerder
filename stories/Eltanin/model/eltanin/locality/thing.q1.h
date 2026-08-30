#pragma once

#include <rmmr/scene/root.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::locality {

    using namespace fqsm::api;

    struct Thing : Entity<Thing> {
        struct Quantum {
            seconds bornAt;
        };
        struct Global {
            seconds now;
            float timeScale;
            optional<rmmr::scene::Root::Id> scene;
        };
        struct Actions : BaseActions {
            static void update(Writing, seconds dt);
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

}
