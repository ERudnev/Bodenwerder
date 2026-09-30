#pragma once

namespace eltanin::cluster::measure {

    // Game lengths = real × this. Game mass scales as this² so GM/r² keeps real g.
    constexpr double celestialFactor = 0.01;

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

    constexpr double au = 1.5e9; // compressed metres
    constexpr double ly = 63000.0 * au;

}
