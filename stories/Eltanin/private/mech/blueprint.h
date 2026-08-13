#pragma once

#include <string>
#include <vector>

#include "semantics/quarks.h"
#include "semantics/shapes.h"
#include "semantics/space.h"

namespace eltanin::mech {

    // Ship schema: cells (frame hosts) + per-cell hull membranes.
    // One cell per lattice volume. Skeleton pieces are local to the cell (no orphans; only thinning).
    // C++: private/mech/blueprint.h — doctrine/mech/blueprint.q1.types
    struct Blueprint {
        struct Cell {
            struct Frame {
                std::vector<skeleton::Corner> corners;
                std::vector<skeleton::Halfrib> halfribs;
            };
            struct Hull {
                std::vector<skeleton::Membrane> membranes;
            };

            space::cell::Pose pose;
            frame::shape shape; // declared intent; population may be thinned
            Frame frame;
            Hull hull;
        };

        std::string name;
        std::string author; // manufacturer
        std::vector<Cell> cells;
    };

}
