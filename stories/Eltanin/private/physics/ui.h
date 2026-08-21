#pragma once

#include "physics/system.h"

#include <eltanin/physics/rigid.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/texpack.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>

#include <string>
#include <vector>

namespace eltanin::phys {

    struct Ui {
        struct ParticleRef {
            rigid::Crystal::Id crystal;
            std::size_t index;
        };
        struct State {
            vector<rmmr::scene::actor::Mesh::Id> actors;
            vector<rmmr::scene::actor::Mesh::Id> particles;
        };
        State state;
        rmmr::resource::material::Asset::Id shapeMaterial;
        rmmr::resource::texpack::Pack::Id shapeTexpack;
        std::string shapeAlbedoLayer;
        rmmr::resource::geometry::Asset::Id shapeGeometry;
        rmmr::resource::geometry::Asset::Id particleGeometry;
        rmmr::resource::material::Asset::Id particleMaterial;

        Ui(rmmr::resource::material::Asset::Id shapeMaterial,
           rmmr::resource::texpack::Pack::Id shapeTexpack,
           std::string shapeAlbedoLayer,
           rmmr::resource::geometry::Asset::Id shapeGeometry,
           rmmr::resource::geometry::Asset::Id particleGeometry,
           rmmr::resource::material::Asset::Id particleMaterial);

        void draw(Writing, bool& open, System&);

    private:
        bool showColliders;
        bool showParticles;
        bool prevShowColliders;
        bool prevShowParticles;

        vector<rigid::Crystal::Id> bodies;
        vector<rmmr::resource::geometry::Asset::Id> colliderGeometries;
        vector<ParticleRef> particleRefs;

        void enableColliders(Writing);
        void disableColliders(Writing);
        void enableParticles(Writing);
        void disableParticles(Writing);
        void syncColliders(Writing);
        void syncParticles(Writing);
    };

}
