#pragma once

#include "semantics/shapes.h"
#include "semantics/space.h"
#include "semantics/slots.h"
#include "semantics/subframe.h"

#include <string>
#include <vector>

namespace eltanin::mech {

    // Construction blueprint (schema): independent layer lists; validity is external.
    // Multiple Cell entries may share a lattice volume when their frame corner sets are disjoint.
    struct Element {
        struct Cell {
            Pose pose;
            frame::shape shape; // construction pattern (k*); drives subframe fill
            slot::inner role;
            // Local to pose: cube lattice of this cell (same space as subframe::recipes).
            std::vector<subframe::Recipe::Corner> corners;
            std::vector<subframe::Recipe::Edge> edges;
            bool subframeBare; // true: keep empty subframe (no recipe re-fill on sync)
        };

        struct Plate {
            Pose pose;
            plate::shape shape;
            slot::plate role;
        };
    };

    struct Blueprint {
        std::string name;
        std::string author;
        std::vector<Element::Cell> cells;
        std::vector<Element::Plate> hull;
    };

}
