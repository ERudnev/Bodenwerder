#pragma once

#include <string>
#include <vector>

#include <eltanin/mech/semantics.q1.h>

// Hand projection of doctrine/mech/blueprint.q1.types.

namespace eltanin::mech {

    // Ship schema: cells with skeleton pieces + membranes.
    // One cell per lattice volume. Pieces are local to the cell (no orphans; only thinning).
    struct Blueprint {
        struct Cell {
            Pose pose;
            frame::shape shape; // declared intent; population may be thinned
            std::vector<skeleton::Corner> corners;
            std::vector<skeleton::Halfrib> halfribs;
            std::vector<skeleton::Membrane> membranes;
        };

        std::string name;
        std::string author; // manufacturer
        std::vector<Cell> cells;
    };

}
