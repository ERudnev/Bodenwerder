#pragma once

#include <string>
#include <vector>

#include "semantics/quarks.h"

namespace eltanin::mech {

    // Ship schema: metadata + grid-frame quarks (knots, chords; attachments later).
    struct Blueprint {
        std::string name;
        std::string author; // manufacturer
        std::vector<quarks::Knot> knots;
        std::vector<quarks::Chord> chords;
    };

}
