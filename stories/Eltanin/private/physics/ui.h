#pragma once

#include "physics/system.h"

#include <eltanin/physics/body.q1.h>
#include <eltanin/physics/rigid.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/texpack.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>

#include <vector>

namespace eltanin::phys {

    struct Ui {
        struct ParticleRef {
            rigid::Crystal::Id crystal;
            std::size_t index;
        };
        struct HullRef {
            Body::Id body;
        };
        struct State {
            vector<rmmr::scene::actor::Mesh::Id> particles;
            vector<rmmr::scene::actor::Mesh::Id> hulls;
        };
        State state;
        rmmr::resource::geometry::Asset::Id particleGeometry;
        rmmr::resource::geometry::Asset::Id sphereGeometry;
        rmmr::resource::material::Asset::Id particleMaterial;
        rmmr::resource::material::Asset::Id hullMaterial;
        rmmr::resource::texpack::Pack::Id hullTexpack;

        Ui(rmmr::resource::geometry::Asset::Id particleGeometry,
           rmmr::resource::geometry::Asset::Id sphereGeometry,
           rmmr::resource::material::Asset::Id particleMaterial,
           rmmr::resource::material::Asset::Id hullMaterial,
           rmmr::resource::texpack::Pack::Id hullTexpack);

        void draw(Writing, bool& open, System&);

    private:
        bool showParticles;
        bool prevShowParticles;
        bool showHulls;
        bool prevShowHulls;

        vector<ParticleRef> particleRefs;
        vector<HullRef> hullRefs;
        vector<rmmr::scene::actor::Mesh::Id> hiddenProduction;

        void enableParticles(Writing, rmmr::scene::Root::Id);
        void disableParticles(Writing);
        void syncParticles(Writing);
        void enableHulls(Writing, rmmr::scene::Root::Id);
        void disableHulls(Writing);
        void syncHulls(Writing);
        void hideProduction(Writing);
        void restoreProduction(Writing);
    };

}
