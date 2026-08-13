#pragma once

#include <vector>

#include <base/maybe.h>
#include <eltanin/mech/blueprint.q1.h>
#include <eltanin/mech/semantics.q1.h>

#include "mech/semantics/shapes.h"

namespace eltanin::views::blueprints::membraneSlots {

    using Cell = mech::Blueprint::Cell;

    // Editor query: free cell face + Membrane to place there (kind+ori). Not blueprint schema.
    struct Slot {
        mech::skeleton::Membrane membrane;
        mech::frame::FaceIndex face;
    };

    // Faces of cell.shape with intact corners and no membrane yet.
    auto possible(const Cell&) -> std::vector<Slot>;

    // Face index for a placed hull membrane. Empty if not on this cell topology.
    auto faceFor(const Cell&, mech::skeleton::Membrane) -> base::maybe<mech::frame::FaceIndex>;

}
