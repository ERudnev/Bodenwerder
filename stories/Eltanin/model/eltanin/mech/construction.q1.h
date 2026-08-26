#pragma once

#include <eltanin/physics/rigid.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::mech {

    using namespace fqsm::api;

    struct Construction {};

    struct Construct : Entity<Construct> {
        struct Quantum {
            Custody<phys::rigid::Crystal> body;
            Custody<rmmr::scene::actor::Mesh> actor;
            Construction construction;
        };
        struct Actions : BaseActions {};
        struct Internals;
        static const Behavior customAspectReactions();
    };

}
