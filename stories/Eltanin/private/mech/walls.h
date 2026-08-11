#pragma once

#include <vector>

#include "blueprint.h"
#include "semantics/quarks.h"
#include "semantics/shapes.h"

namespace eltanin::mech {

    // Candidate membrane on a cell face (local ori in cell space). Corners must be intact; half-chords may be thinned.
    struct WallSlot {
        quarks::Wall wall;
        frame::FaceIndex face;
    };

    // Corners present in the cell from live knots (pivot → cornerIndex(localOri, 0)).
    auto occupiedCorners(const Blueprint::Cell&) -> std::vector<bool>; // size 8

    // Faces of cell.shape whose loops are fully occupied; skips walls already in cell.hull.
    auto possibleWalls(const Blueprint::Cell&) -> std::vector<WallSlot>;

}
