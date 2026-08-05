#pragma once

#include "semantics/shapes.h"
#include "semantics/space.h"
#include "semantics/slots.h"

#include <string>
#include <vector>

namespace eltanin::mech {

    // Construction blueprint (schema): independent layer lists; validity is external.
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

        struct Stub {
            Pose pose;
            wing::shape shape;
            slot::wing role;
        };
    };

    struct Blueprint {
        std::string name;
        std::string author;
        std::vector<Element::Cell> cells;
        std::vector<Element::Stub> stubs;
        std::vector<Element::Plate> hull;
    };

}
