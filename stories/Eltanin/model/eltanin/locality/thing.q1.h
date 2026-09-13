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
            rmmr::scene::Root::Id scene;
        };
        struct Always {
            static auto assemble(SettingUp&) -> Global;
        };
        struct Actions : BaseActions {
            static void update(Writing, seconds dt);
            static void followBodies(Stewarding);
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

}
