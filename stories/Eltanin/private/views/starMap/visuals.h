#pragma once

#include <vector>

#include <base/maybe.h>
#include <rmmr/math.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/scene/actors/family.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/gizmos.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/system/window.q1.h>

#include "cluster/astronomy.h"

#include <fQSM/api/interface.h>

namespace eltanin::views::starmap {

    using namespace fqsm::api;

    struct Marker {
        struct Drop {
            rmmr::scene::actor::Mesh::Id actor;
            integer dashes;
        };
        rmmr::scene::actor::Mesh::Id reticle;
        Drop dropX;
        Drop dropY;
        Drop dropZ;
    };

    struct Visuals {
        struct Lod {
            rmmr::scene::actor::Family::Id family;
            rmmr::resource::geometry::Asset::Id mesh;
        };
        struct Star {
            cluster::Celestial::Id celestial;
            rmmr::scene::actor::Replica::Id coarse;
            rmmr::scene::actor::Replica::Id fine;
            float celestialRadius;
        };
        struct Radial {
            cluster::Celestial::Id celestial;
            rmmr::scene::actor::Mesh::Id plane;
            rmmr::scene::actor::Mesh::Id pole;
        };
        struct Lattice {
            rmmr::scene::Grid::Id grid;
            float cellLy;
        };

        base::maybe<Marker> currentPlayer;
        base::maybe<Marker> viewFocus;
        vector<Lattice> lattices;
        base::maybe<rmmr::scene::actor::Mesh::Id> axisX;
        base::maybe<rmmr::scene::actor::Mesh::Id> axisY;
        base::maybe<rmmr::scene::actor::Mesh::Id> axisZ;
        vector<rmmr::resource::geometry::Asset::Id> dashMeshes;
        base::maybe<rmmr::resource::material::Asset::Id> gizmo;
        base::maybe<rmmr::resource::material::Asset::Id> starMaterial;
        base::maybe<rmmr::resource::material::Asset::Id> radialMaterial;
        base::maybe<rmmr::resource::geometry::Asset::Id> radialPlane;
        base::maybe<Lod> coarse;
        base::maybe<Lod> fine;
        vector<Star> stars;
        vector<Radial> radials;
        base::maybe<rmmr::system::Window::Id> renderWindow;
        rmmr::Pos player;
        rmmr::Pos focus;
        float scaleLy;
        struct Display {
            bool grid;
        } display;

        static auto starMeshRadius(float celestialRadius, float cameraDistance, float pixelWorld) -> float;
        auto addAssets(Writing) -> bool;
        auto place(Writing, rmmr::scene::Root::Id, rmmr::system::Window::Id) -> bool;
        auto bind(Writing, rmmr::scene::Root::Id, const cluster::Astronomy&) -> bool;
        void follow(Writing, rmmr::scene::Camera::Id);
    };

}
