#pragma once

namespace eltanin::mech {

    // Level 0 — how a piece lives on the ship (not form, not function).
    enum class layer {
        plate, // skin tile on the outside of frame volumes (fairings, armor panels, …)
        frame, // volumetric skeleton; cells that dock frame↔frame
        inner, // mechanism inscribed in frame cell(s); binds to the cell lattice
        wing, // two-sided thing, able to connect to frame with at least 2 points
    };

}
