#pragma once

#include <vector>

#include <base/maybe.h>
#include <rmmr/math.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/gizmos.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/system/window.q1.h>

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
        base::maybe<Marker> currentPlayer;
        base::maybe<Marker> viewFocus;
        base::maybe<rmmr::scene::Grid::Id> tensGrid;
        base::maybe<rmmr::scene::Grid::Id> unitGrid;
        base::maybe<rmmr::scene::actor::Mesh::Id> axisX;
        base::maybe<rmmr::scene::actor::Mesh::Id> axisY;
        base::maybe<rmmr::scene::actor::Mesh::Id> axisZ;
        vector<rmmr::resource::geometry::Asset::Id> dashMeshes;
        base::maybe<rmmr::resource::material::Asset::Id> unlit;
        rmmr::Pos player;
        rmmr::Pos focus;
        float scaleLy;

        auto place(Writing, rmmr::scene::Root::Id, rmmr::system::Window::Id) -> bool;
        void follow(Writing, rmmr::scene::Camera::Id);
    };

}
