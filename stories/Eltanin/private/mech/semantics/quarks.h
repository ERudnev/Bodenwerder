#pragma once

#include <vector>

#include "shapes.h"
#include "space.h"
#include "subframe.h"

namespace eltanin::mech::quarks {

    // Atomic frame piece in a cell. Not placed by hand as a lone editor stroke —
    // cell recipes (e.g. "k7 in this cell") expand into several Knots in that cell.
    // Mesh sits at cube corner 0 in its local frame; pose.ori places it in the cell.
    struct Knot {
        using Kind = subframe::corner::kind;

        Kind kind;
        space::cell::Pose pose;
    };

    // Half-stick quark in a cell (one pole of an edge). Seat in the cell cube comes from
    // interframe Entry.origin (s → 0, e → ray end); pole selects …s / …e mesh.
    struct HalfChord {
        using Kind = subframe::halfEdge::kind;

        Kind kind;
        subframe::halfEdge::Pole pole;
        space::cell::Pose pose;
    };

    struct Wall {
        using Kind = subframe::membrane::kind;

        Kind kind;
        space::cell::Pose pose;
    };

    // Expand frame::shape at a cell pose → knots / half-chords in that cell (subframe::recipes).
    auto seedCorners(frame::shape, space::cell::Pose) -> std::vector<Knot>;
    auto seedHalfChords(frame::shape, space::cell::Pose) -> std::vector<HalfChord>;

}
