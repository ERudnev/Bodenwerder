#pragma once

#include <vector>

#include "shapes.h"
#include "space.h"
#include "subframe.h"

namespace eltanin::mech::quarks {

    // Local roll inside a Cell (recipe-space; cell.ori = 0). World ori = compose(cell.ori, local).
    using LocalOri = rmmr::renderer::Signed32;

    // Atomic frame piece. Seeded with a cell; not spawned as a lone editor stroke.
    // Mesh sits at cube corner 0 in its local frame; ori places it in the cell.
    struct Knot {
        using Kind = subframe::corner::kind;

        Kind kind;
        LocalOri ori;
    };

    // Half-stick quark (one pole of an edge). Seat from interframe Entry.origin; pole selects …s / …e.
    struct HalfChord {
        using Kind = subframe::halfEdge::kind;

        Kind kind;
        subframe::halfEdge::Pole pole;
        LocalOri ori;
    };

    struct Wall {
        using Kind = subframe::membrane::kind;

        Kind kind;
        LocalOri ori;
    };

    // World discrete pose for a quark: cell lattice pos + compose(cell.ori, local).
    auto worldPose(const space::cell::Pose& cell, LocalOri local) -> space::cell::Pose;

    // Expand frame::shape → local knots / half-chords (subframe::recipes, cell ori 0).
    auto seedCorners(frame::shape) -> std::vector<Knot>;
    auto seedHalfChords(frame::shape) -> std::vector<HalfChord>;

}
