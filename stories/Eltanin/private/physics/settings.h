#pragma once

#include <fQSM/api/interface.h>

namespace eltanin::phys {

    using namespace fqsm::api;

    struct Settings {
        static constexpr float constraintStiffness = 1.0f;//0.75f; // Hitman-style goal pull (constraints)
        static constexpr float isaAirDensity = 1225.0f; // g/m³ ISA
        static constexpr float airDragTau = 1.0f; // seconds to e-fold linear speed at isaAirDensity
        static constexpr float airSpinHalfLife = 3.0f; // Solid ω halves in this many seconds at isaAirDensity
        static constexpr float restLinear = 1.0e-5f; // m/tick; below this (x−prev) is zeroed
        static constexpr float solidLiveSpeed = 0.1f; // m/s; Solid↔Crystal — soft fade of restitution and spin below this closing speed
        static constexpr float cohesionWound = 2.5f; // Crystal faces: Δcohesion = cohesionWound · |p| / m_face; 30mm 0.4 kg × 200 m/s vs 4 t plate → 5%
        static constexpr float boxCohesionWound = 25.0f; // Solid boxes (scrap): same law on whole-body mass; 10× so a 24 t dummy cube splits like a 4 t plate
        static constexpr float hullShedKelvin = 2500.0f; // Thermal shed → Dust, not scrap split.
        static constexpr float scrapVaporKelvin = 3400.0f; // wreckage gone; later: melt VFX / secondary flash
        static constexpr seconds fixedStep = 0.01;
        static constexpr seconds thermalStep = 0.05; // 20 Hz; Construct heats already uploaded every fixedStep via followBody
        static constexpr seconds hullCool = 40; // Construct radiate; ~10× Dust. Each thermal step T *= (1 − dt/τ)²
        static constexpr float skyKelvin = 2.7f; // CMB default; live ambient is Root.atmosphereTemperature
        static constexpr float radiateSigma = 5.0e-13f; // parrot mass × kelvin; five times slower than the first lava-scale guess
    };

}
