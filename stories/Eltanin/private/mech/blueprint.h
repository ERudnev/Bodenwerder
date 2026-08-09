#pragma once

#include <string>
#include <vector>

#include "semantics/quarks.h"

namespace eltanin::mech {

    // Ship schema: metadata + grid-frame quarks (knots first; sticks / attachments later).
    struct Blueprint {
        std::string name;
        std::string author; // manufacturer
        std::vector<quarks::Knot> knots;
    };

}
