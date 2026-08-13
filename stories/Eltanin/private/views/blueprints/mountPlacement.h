#pragma once

#include <cstddef>
#include <vector>

#include <base/maybe.h>
#include <base/types/common_types.h>
#include <eltanin/mech/blueprint.q1.h>
#include <eltanin/mech/semantics.q1.h>
#include <rmmr/math.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/root.q1.h>

#include "mech/semantics/shapes.h"

#include <fQSM/api/interface.h>

namespace eltanin::views::blueprints::mountPlacement {

    using namespace fqsm::api;
    using Blueprint = mech::Blueprint::Quantum;
    using Cell = mech::Blueprint::Cell;

    struct MouseRay {
        rmmr::Pos origin;
        rmmr::vec3 dir;
    };

    // Hang-mounts mode: ray → cell face → absolute grid points; balls visualize the set.
    struct Cursor {
        bool enabled;
        base::maybe<std::size_t> cell;
        base::maybe<mech::frame::FaceIndex> face;
        std::vector<base::common_types::index3> points;
        std::vector<rmmr::scene::actor::Mesh::Id> balls;
        base::maybe<rmmr::resource::geometry::Asset::Id> sphere;
        base::maybe<::rmmr::resource::material::Asset::Id> material;
    };

    void resetAim(Cursor&);
    void clearBalls(Writing, rmmr::scene::Root::Id root, Cursor&);

    // Absolute lattice corners of a cell face (cell index + oriented unit-cube corner).
    auto faceGridPoints(const Cell&, mech::frame::FaceIndex) -> std::vector<base::common_types::index3>;

    // Closest face under ray → Cursor.points (absolute grid). Empty aim if miss.
    void aim(Cursor&, const Blueprint&, const MouseRay&);

    // Spawn / move sphere actors at points * edge2meters.
    void syncBalls(Writing, rmmr::scene::Root::Id root, Cursor&);

}
