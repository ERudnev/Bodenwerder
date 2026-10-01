#pragma once

namespace eltanin::cluster::measure {

    struct Mass {
        static constexpr double sun = 1.9884e30;
        static constexpr double earth = 5.9722e24;
    };

    struct Radius {
        static constexpr double sun = 6.96e8;
        static constexpr double earth = 6.371e6;
    };

    struct Temperature {
        static constexpr float sun = 5772.0f;
    };

    struct ShrinkFactor {
        static constexpr double celestial = 100.0;
    };

    constexpr double eMm = 1000000.0 / ShrinkFactor::celestial;
    constexpr double eAU = 50000.0 * eMm; // extra ×3 on interplanetary vs celestial-only AU
    constexpr double eLY = 500.0 * eAU; // extra ×126 on interstellar (SI ~63000 AU/ly → 500 eAU)

}

namespace eltanin {
    using cluster::measure::Mass;
    using cluster::measure::Radius;
    using cluster::measure::ShrinkFactor;
    using cluster::measure::Temperature;
    using cluster::measure::eAU;
    using cluster::measure::eLY;
    using cluster::measure::eMm;
}
