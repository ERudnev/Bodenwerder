#pragma once

#include <eltanin/mech/construction.q1.h>
#include <eltanin/physics/rigid.q1.h>

namespace eltanin::phys {

    // After collisions, before final Horn. Frame wave (knots+ribs+membranes), rib pin, then unbolt plates/volumes.
    void reconcile(const mech::Construction&, rigid::Crystal::Quantum&, const Body::Quantum&);

}
