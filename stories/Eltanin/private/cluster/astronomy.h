#pragma once

#include <vector>

#include <eltanin/cluster/orbital.q1.h>
#include <eltanin/cluster/space.q1.h>
#include <eltanin/cluster/celestial.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::cluster {

    using namespace fqsm::api;

    struct Astronomy {
        vector<Axis::Id> axes;
        vector<Celestial::Id> celestials;

        void generate(Writing);
        void generateSol(Writing, space::Position);
    };

}
