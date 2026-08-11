#pragma once

#include <string>
#include <vector>

#include "semantics/quarks.h"

namespace eltanin::mech {

    // Ship schema: frame quarks + hull membranes (outfit-side, not load-bearing; not auto-seeded from frame).
    // C++: private/mech/blueprint.h — doctrine/mech/blueprint.q1.types
    struct Blueprint {
        struct Frame {
            std::vector<quarks::Knot> knots;
            std::vector<quarks::HalfChord> halfChords;
        };
        struct Hull {
            std::vector<quarks::Wall> walls;
        };

        std::string name;
        std::string author; // manufacturer
        Frame frame;
        Hull hull;
    };

}
