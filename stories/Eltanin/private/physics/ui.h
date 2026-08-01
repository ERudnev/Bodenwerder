#pragma once

#include "physics/system.h"

#include <eltanin/physics/atomic.q1.h>
#include <eltanin/physics/particle.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/scene/actors/simple.q1.h>

#include <base/maybe.h>

#include <vector>

namespace eltanin::phys {

    // ImGui panel for the physics subsystem (not Q1).
    struct Ui {
        struct State {
            vector<rmmr::scene::actor::Simple::Id> actors;
            vector<rmmr::scene::actor::Simple::Id> particles;
        };
        State state;
        base::maybe<rmmr::resource::material::Asset::Id> shapeMaterial;
        base::maybe<rmmr::resource::geometry::Asset::Id> particleGeometry;
        base::maybe<rmmr::resource::material::Asset::Id> particleMaterial;

        void draw(Writing, bool& open, System&);

    private:
        bool prevOpen = false;
        vector<Atomic::Id> bodies;
        vector<Particle::Id> particleIds;

        void onDrawEnabled(Writing);
        void onDrawDisabled(Writing);
        void onDraw(Writing);
    };

}
