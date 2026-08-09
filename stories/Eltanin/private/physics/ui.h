#pragma once

#include "physics/system.h"

#include <eltanin/physics/atomic.q1.h>
#include <eltanin/physics/particle.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/texpack.q1.h>
#include <rmmr/scene/actors/simple.q1.h>

#include <base/maybe.h>

#include <string>
#include <vector>

namespace eltanin::phys {

    struct Ui {
        struct State {
            vector<rmmr::scene::actor::Simple::Id> actors;
            vector<rmmr::scene::actor::Simple::Id> particles;
        };
        State state;
        base::maybe<rmmr::resource::material::Asset::Id> shapeMaterial;
        base::maybe<rmmr::resource::texpack::Pack::Id> shapeTexpack;
        base::maybe<std::string> shapeAlbedoLayer;
        base::maybe<rmmr::resource::geometry::Asset::Id> shapeGeometry;
        base::maybe<rmmr::resource::geometry::Asset::Id> particleGeometry;
        base::maybe<rmmr::resource::material::Asset::Id> particleMaterial;

        void draw(Writing, bool& open, System&);

    private:
        bool showColliders = false;
        bool showParticles = false;
        bool prevShowColliders = false;
        bool prevShowParticles = false;

        vector<Atomic::Id> bodies;
        vector<Particle::Id> particleIds;

        void enableColliders(Writing);
        void disableColliders(Writing);
        void enableParticles(Writing);
        void disableParticles(Writing);
        void syncColliders(Writing);
        void syncParticles(Writing);
    };

}
