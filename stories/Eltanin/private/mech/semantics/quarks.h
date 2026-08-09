#pragma once

#include <vector>

#include "shapes.h"
#include "subframe.h"

namespace eltanin::mech::quarks {

    // Atomic frame piece on the grid. Not placed by hand as a lone editor stroke —
    // cell recipes (e.g. "k7 in this cell") expand into several Knots on the surrounding nodes.
    struct Knot {
        using Kind = subframe::corner::kind;

        Kind kind;
        space::grid::Pose pose;

        // Recover cell-space seat for collision / merge. See quarks.cpp — not a pure inverse of grid.
        auto evaluateCellPlacement() const -> space::cell::Placement;
    };

    // Expand frame::shape at a cell pose → knots on grid nodes (reads subframe::recipes).
    auto seedCorners(frame::shape, space::cell::Pose) -> std::vector<Knot>;

}
