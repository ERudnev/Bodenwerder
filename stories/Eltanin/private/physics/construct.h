#pragma once

#include <eltanin/mech/construction.q1.h>
#include <eltanin/physics/rigid.q1.h>

namespace eltanin::phys {

    // After collisions, before final Horn. Restore Construction edge lengths on the Crystal vertex buffer.
    void reconcile(const mech::Construction&, rigid::Crystal::Quantum&);

}
