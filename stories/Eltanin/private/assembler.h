#pragma once

#include <eltanin/mech/blueprint.q1.h>
#include <fQSM/api/interface.h>
#include <rmmr/math.q1.h>

namespace eltanin {

    using namespace fqsm::api;

    struct Assembler {
        static void immediateSpawn(Writing, mech::Blueprint::Id, rmmr::Pose);
    };

}
