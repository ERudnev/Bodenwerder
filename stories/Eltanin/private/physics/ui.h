#pragma once

#include "physics/system.h"

#include <eltanin/physics/atomic.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/scene/actors/simple.q1.h>

#include <base/maybe.h>

#include <vector>

namespace eltanin::phys {

    // ImGui panel for the physics subsystem (not Q1).
    struct Ui {
        struct State {
            vector<rmmr::scene::actor::Simple::Id> actors;
        };
        State state;
        base::maybe<rmmr::resource::material::Asset::Id> shapeMaterial;

        void draw(Writing, bool& open, System&);

    private:
        bool prevOpen = false;
        vector<Atomic::Id> bodies;

        void onDrawEnabled(Writing);
        void onDrawDisabled(Writing);
        void onDraw(Writing);
    };

}
