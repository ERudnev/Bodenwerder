#pragma once

#include <fQSM/api/interface.h>

namespace eltanin::phys {

    using namespace fqsm::api;

    struct Settings {
        enum class DebrisCohort {
            individual,
            families,
            unified,
        };

        // Class → world. spawnAsExplosion(1) = sailor at the navel; 10 = half a frigate if inside.
        struct Explosions {
            static constexpr float kineticMeters = 12.0f; // kinetic radius at class 1; higher classes scale as sqrt
            static constexpr float kineticCore = 0.01f; // visual shock seed; 0.1 m at class 10
            static constexpr float kineticPascal = 5.0e6f; // Pa at class 1, atten 1
            static constexpr float frontSpeed = 100.0f; // m/s; duration = radius / this
            static constexpr float victimDuration = 0.70f; // victim speed cap = radius / this
            static constexpr float kineticSpin = 3.0f; // cheat tumble gain on the kinetic front
            static constexpr float thermalKelvin0 = 2800.0f;
            static constexpr float thermalKelvinPer = 200.0f;
            static constexpr float thermalMeters = 1.0f; // visual plasma radius at class 1
            static constexpr seconds thermalDuration = 1.0;
            static constexpr float thermalHalo = 2.5f; // soak in plasma radii; visual ball stays thermalMeters
            static constexpr float brisanceYield = 8.0f; // cohesion wound at class 1
            static constexpr float brisanceRadius = 0.85f; // vs kineticMeters
            static constexpr seconds brisanceDuration = 1.0;
        };

        struct Air {
            static constexpr float isaDensity = 1225.0f; // g/m³ ISA
            static constexpr float dragTau = 1.0f; // seconds to e-fold linear speed at isaDensity
            static constexpr float spinHalfLife = 3.0f; // Solid ω halves in this many seconds at isaDensity
        };

        struct Cohesion {
            static constexpr float wound = 2.5f; // Crystal faces: Δcohesion = wound · |p| / m_face; 30mm 0.4 kg × 200 m/s vs 4 t plate → 5%
            static constexpr float boxWound = 25.0f; // Solid boxes (scrap): same law on whole-body mass; 10× so a 24 t dummy cube splits like a 4 t plate
        };

        struct Heat {
            static constexpr float hullShedKelvin = 2500.0f; // Thermal shed → Dust, not scrap split.
            static constexpr float scrapVaporKelvin = 3400.0f; // wreckage gone; later: melt VFX / secondary flash
            static constexpr seconds hullCool = 40; // Construct radiate; ~10× Dust. Each thermal step T *= (1 − dt/τ)²
            static constexpr float skyKelvin = 2.7f; // CMB default; live ambient is Root.atmosphereTemperature
            static constexpr float radiateSigma = 5.0e-13f; // parrot mass × kelvin; five times slower than the first lava-scale guess
        };

        static constexpr float constraintStiffness = 1.0f;//0.75f; // Hitman-style goal pull (constraints)
        static constexpr float restLinear = 1.0e-5f; // m/tick; below this (x−prev) is zeroed
        static constexpr float solidLiveSpeed = 0.1f; // m/s; Solid↔Crystal — soft fade of restitution and spin below this closing speed
        static constexpr seconds fixedStep = 0.01;
        static constexpr seconds thermalStep = 0.05; // 20 Hz; Construct heats already uploaded every fixedStep via followBody
        static inline DebrisCohort debrisCohort = DebrisCohort::unified;
    };

}
