#pragma once

#include <eltanin/mech/semantics.q1.h>

#include <map>

namespace eltanin::mech::physical {

    using skeleton::Corner;
    using skeleton::Halfrib;
    using skeleton::Membrane;

    constexpr float knotShell = 0.0f;
    constexpr float ribShell = 0.2f;
    constexpr float membraneShell = 0.2f;

    inline const std::map<Corner::Kind, float> knotQuarkMass{
        {Corner::Kind::c124, 125.0f},
        {Corner::Kind::c1364, 125.0f},
        {Corner::Kind::c164, 125.0f},
        {Corner::Kind::c134, 125.0f},
        {Corner::Kind::c135, 125.0f},
        {Corner::Kind::c12, 125.0f},
        {Corner::Kind::c13, 125.0f},
        {Corner::Kind::c15, 125.0f},
        {Corner::Kind::c16, 125.0f},
        {Corner::Kind::c34, 125.0f},
        {Corner::Kind::c35, 125.0f},
    };

    inline const std::map<Halfrib::Kind, float> halfribQuarkMass{
        {Halfrib::Kind::he1deg90, 250.0f},
        {Halfrib::Kind::he1deg45, 250.0f},
        {Halfrib::Kind::he3deg71, 250.0f},
        {Halfrib::Kind::he3deg90, 250.0f},
        {Halfrib::Kind::he3deg125, 250.0f},
    };

    // p1111 = 4 m square → 1000 kg. p121 half. p2121 parallelogram √2. p222* triangles √3/2.
    inline const std::map<Membrane::Kind, float> membraneQuarkMass{
        {Membrane::Kind::u1111, 1000.0f},
        {Membrane::Kind::u121, 500.0f},
        {Membrane::Kind::u2121, 1400.0f},
        {Membrane::Kind::u222A, 870.0f},
        {Membrane::Kind::u222V, 870.0f},
    };

}
