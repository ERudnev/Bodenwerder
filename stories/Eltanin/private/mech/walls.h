#pragma once

#include <vector>

#include <base/maybe.h>

#include "blueprint.h"
#include "semantics/quarks.h"
#include "semantics/shapes.h"

namespace eltanin::mech {

    // Candidate membrane on a cell face (local ori in cell space). Corners must be intact; halfribs may be thinned.
    struct WallSlot {
        skeleton::Membrane membrane;
        frame::FaceIndex face;
    };

    // Cube vertices present from live skeleton::Corner (pivot → cornerIndex(localOri, 0)).
    auto occupiedCorners(const Blueprint::Cell&) -> std::vector<bool>; // size 8

    // Faces of cell.shape whose loops are fully occupied; skips membranes already in cell.hull.
    auto possibleWalls(const Blueprint::Cell&) -> std::vector<WallSlot>;

    // Face index for a placed hull membrane (kind + local ori). Empty if not on this cell topology.
    auto faceForWall(const Blueprint::Cell&, skeleton::Membrane) -> base::maybe<frame::FaceIndex>;

}
