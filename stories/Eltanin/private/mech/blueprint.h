#pragma once

#include "semantics/shapes.h"
#include "semantics/space.h"
#include "semantics/slots.h"

#include <string>
#include <vector>

namespace eltanin::mech {

    // Construction blueprint (schema): independent layer lists; validity is external.
    // Multiple Cell entries may share a lattice volume when their frame corner sets are disjoint.
    struct Element {
        struct Cell {
            Pose pose;
            frame::shape shape;
            slot::inner role;
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
