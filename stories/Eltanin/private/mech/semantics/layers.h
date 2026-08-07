#pragma once

namespace eltanin::mech {

    // Level 0 — how a piece lives on the ship (not form, not function).
    enum class layer {
        plate, // skin tile on the outside of frame volumes (fairings, armor panels, …)
        frame, // skeleton piece in a cell volume (volumetric k* or flat k*f*)
        inner, // mechanism inscribed in frame cell(s); binds to the cell lattice
    };

}
