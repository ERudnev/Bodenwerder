#pragma once

#include <base/maybe.h>
#include <geo/celestial/planetiod.h>
#include <rmmr/scene/root.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::locality {

    using namespace fqsm::api;

    using Landscape = geo::Landscape;

    struct Thing : Entity<Thing> {
        struct Quantum {
            seconds bornAt;
        };
        struct Global {
            seconds now;
            float timeScale;
            rmmr::scene::Root::Id scene;
            base::maybe<Landscape> landscape;
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
