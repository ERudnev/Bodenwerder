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
    };

    // Half-stick quark on the grid (one pole of an edge). Same spirit as Knot: kind + pose.
    // pole selects interframe …s / …e mesh; glue later may merge up to 8 Chords into one rod.
    struct Chord {
        using Kind = subframe::halfEdge::kind;

        Kind kind;
        subframe::halfEdge::Pole pole;
        space::grid::Pose pose;
    };

    // Expand frame::shape at a cell pose → knots / chords on grid (reads subframe::recipes).
    auto seedCorners(frame::shape, space::cell::Pose) -> std::vector<Knot>;
    auto seedChords(frame::shape, space::cell::Pose) -> std::vector<Chord>;

}
