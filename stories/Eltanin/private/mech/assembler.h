#pragma once

#include <eltanin/mech/blueprint.q1.h>
#include <eltanin/mech/construction.q1.h>
#include <fQSM/api/interface.h>
#include <rmmr/math.q1.h>
#include <rmmr/scene/root.q1.h>

namespace eltanin::mech {

    using namespace fqsm::api;

    struct Assembler {
        static auto spawn(Writing, rmmr::scene::Root::Id, rmmr::Pose, Blueprint::Id) -> Construct::Id;
    };

}
