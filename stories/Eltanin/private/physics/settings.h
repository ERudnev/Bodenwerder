#pragma once

#include <fQSM/api/interface.h>

namespace eltanin::phys {

    using namespace fqsm::api;

    struct Settings {
        enum class Debris {
            scrap,
            dust,
        };

        // Scrap only. Construct islands after a break are always their own compound dad.
        enum class DebrisCohort {
            individual,
            families,
            unified,
        };

        // Flash::Channels are meters (radii). Intensity still follows the old class law via ref spans below.
        struct Explosions {
            static constexpr float kineticMeters = 12.0f; // intensity ref: former class 1 kinetic radius
            static constexpr float kineticCore = 0.01f; // visual shock seed × former class (~ (R/ref)^2)
            static constexpr float kineticPascal = 5.0e6f; // Pa at former class 1, atten 1
            static constexpr float frontSpeed = 100.0f; // m/s; duration = radius / this
            static constexpr float victimDuration = 0.70f; // victim speed cap = radius / this
            static constexpr float kineticSpin = 3.0f; // cheat tumble gain on the kinetic front
            static constexpr float thermalKelvin0 = 2800.0f;
            static constexpr float thermalKelvinPer = 200.0f;
            static constexpr float thermalMeters = 1.0f; // intensity ref: former class 1 plasma radius
            static constexpr seconds thermalDuration = 2.0;
            static constexpr float thermalHalo = 2.5f; // soak in plasma radii
            static constexpr float brisanceYield = 8.0f; // cohesion wound at former class 1
            static constexpr float brisanceRadius = 0.85f; // intensity ref = kineticMeters × this
            static constexpr seconds brisanceDuration = 0.2;
        };

        struct Air {
            static constexpr float isaDensity = 1225.0f; // g/m³ ISA
            static constexpr float dragTau = 1.0f; // seconds to e-fold linear speed at isaDensity
            static constexpr float spinHalfLife = 3.0f; // Solid ω halves in this many seconds at isaDensity
        };

        struct Cohesion {
            static constexpr float wound = 2.5f; // hit HP: Δcohesion = wound · |p| / m_face; 30mm 0.4 kg × 200 m/s vs 4 t plate → 5%. Live only if constructCollisionWounds
            static constexpr float boxWound = 25.0f; // Solid boxes (scrap): same law on whole-body mass; 10× so a 24 t dummy cube splits like a 4 t plate
            static inline float breakStrain = 0.127f; // rib |ΔL|/L0 this tick before pin; above → that rib primitive cohesion 0. 0.25 = 1 m on a 4 m beam. Not hit HP.
            static inline float peelPlate = 0.2f; // hull plate unbolt vs frame knot COM after the wave; cohesion 0 = unbolt (scrap born at 1)
            static inline float peelMount = 0.4f; // volume mounts (engine / reactor / battery); twice the plate slack
            static constexpr float scrapWear = 0.2f; // /s; any Solid contact this locality update
            static constexpr float scrapDust = 1.0f; // cohesion < −this → Dust
        };

        struct Heat {
            static constexpr float hullShedKelvin = 2500.0f; // Thermal shed → Dust, not scrap split.
            static constexpr float scrapVaporKelvin = 3400.0f; // wreckage gone; later: melt VFX / secondary flash
            static constexpr seconds hullCool = 40; // Construct radiate; ~10× Dust. Each thermal step T *= (1 − dt/τ)²
            static constexpr float skyKelvin = 2.7f; // CMB default; live ambient is Root.atmosphereTemperature
            static constexpr float radiateSigma = 5.0e-13f; // parrot mass × kelvin; five times slower than the first lava-scale guess
        };

        struct Resting {
            static inline bool enabled = true;
            static inline float captureSeconds = 0.5f;
            static inline float captureMeters = 0.1f; // local contact on each body vs the start of the probe, not Body origins
            static inline float captureRadians = 0.04f;
            static constexpr float tensileMeters = 0.25f;
            static constexpr float adhesiveSpeed = 0.5f; // extra grip as equivalent closing speed
            static constexpr float staticFriction = 1.2f;
            static constexpr float twistRadians = 0.35f;
            static constexpr integer solveIterations = 3;
            static inline float dissipate = 0.35f; // pull toward the captured mutual shape; 0 = off, 1 = full step before mass split
            static inline float dissipateResilience = 0.85f; // semiKick; 1 = teleport, 0 = invent Δv like Resilience::shapePull
        };

        // Verlet semiKick: 0 = only current (ball), 1 = current and previous (teleport), ½ = halfKick.
        struct Resilience {
            static constexpr double wave = 0.5; // knot wave along the frame
            static constexpr double ribRestore = 0.5; // semiKick of rib length restore; 0 invents Δv, 1 teleports
            static constexpr double shapePull = 0.5; // semiKick of pullToShape; 0 invents Δv, 1 teleports
            static constexpr double faceSupport = 0.0; // solid/ray recoil onto crystal hull vertices
            static constexpr double solidContact = 0.5; // solid ↔ crystal impact end; rest blends toward 1 (teleport)
            static constexpr double solidSolid = 0.5; // solid ↔ solid positional remaining
            static constexpr double crystalContact = 0.5; // crystal particle vs frozen hull
        };

        static constexpr float constraintStiffness = 1.0f; // k in rib length restore
        static constexpr float shapePull = 0.8f; // k in pullToShape toward pose*shape; <1 leaves contact residual for the next tick
        static constexpr float restLinear = 1.0e-5f; // m/tick; below this (x−prev) is zeroed
        static constexpr float solidLiveSpeed = 0.1f; // m/s; Solid↔Crystal live = vn/(vn+this); semiKick 1→solidContact, friction
        static constexpr seconds fixedStep = 0.02; // TODO: consider 0.012 - 0.015 for better performance
        static constexpr seconds thermalStep = 0.05; // 20 Hz; Construct heats uploaded from update when Crystal.visualHurtStale
        static inline Debris debris = Debris::scrap;
        static inline DebrisCohort debrisCohort = DebrisCohort::individual;
        static inline bool constructCollisionWounds = false; // scarFace → Construct cohesion / shed; Flash unchanged
        static inline bool knotWave = true; // spreadKnotWave along the frame; off = rib restore only
        static inline bool peelSkin = true; // unbolt plates/volumes that lag the frame; membranes ride the wave
    };

}
