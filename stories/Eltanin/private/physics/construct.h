#pragma once

#include <eltanin/mech/construction.q1.h>
#include <eltanin/physics/rigid.q1.h>

namespace eltanin::phys {

    // After collisions, before final Horn. Frame wave along ribs, then rib edge restore (skin is not a load path).
    void reconcile(const mech::Construction&, rigid::Crystal::Quantum&, const Body::Quantum&);

}
