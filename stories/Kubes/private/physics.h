#pragma once

#include <base/maybe.h>
#include <kubes/physics/atom.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/scene/root.q1.h>

#include <fQSM/api/interface.h>

#include <vector>

namespace kubes::phys {

    using namespace fqsm::api;
    using namespace rmmr;

    // Private physics subsystem (not Q1). Bind scene/mesh/material after Product setup.
    struct System {
        struct State {
            bool visualise = false;
        };
        State state;

        void bind(scene::Root::Id, resource::geometry::Asset::Id, resource::material::Asset::Id);
        void drawUi(Writing);
        void step(Stewarding, int64 dt_us);
        Atom::Id addParticle(Writing, vec3 pos);

    private:
        struct Bound {
            scene::Root::Id scene;
            resource::geometry::Asset::Id geometry;
            resource::material::Asset::Id material;
        };

        State prevState;
        base::maybe<Bound> bound;
        std::vector<Visual::Id> visuals;

        void createVisuals(Writing);
        void destroyVisuals(Writing);
        auto spawnVisual(Writing, Atom::Id, const Atom::Quantum&) -> Visual::Id;
    };

}
