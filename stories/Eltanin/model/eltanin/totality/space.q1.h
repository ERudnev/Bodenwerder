#pragma once

#include <rmmr/math.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::totality::space {

    using namespace fqsm::api;

    using Position = dvec3;

    struct Pose {
        Position position;
        dquat orientation;
    };

}
