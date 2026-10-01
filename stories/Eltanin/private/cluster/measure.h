#pragma once

#include <string>

namespace eltanin::cluster::measure {

    struct StretchFactor {
        static constexpr double celestial = 0.02;
        static constexpr double relief = 4.0;
    };

    constexpr double eMm = 1000000.0 * StretchFactor::celestial;
    constexpr double eAU = 100000.0 * eMm;
    constexpr double eLY = 1000.0 * eAU;

    struct Mass {
        static constexpr double sun = 1.9884e30;
        static constexpr double earth = 5.9722e24;
    };

    struct Radius {
        static constexpr double sun = 696.0 * eMm;
        static constexpr double earth = 6.371 * eMm;
    };

    struct Temperature {
        static constexpr float sun = 5772.0f;
    };

    struct Format {
        static auto altitude(double metres) -> std::string;
        static auto distance(double metres) -> std::string;
        static auto age(double myr) -> std::string;
    };

}

namespace eltanin {
    using cluster::measure::Mass;
    using cluster::measure::Radius;
    using cluster::measure::StretchFactor;
    using cluster::measure::Temperature;
    using cluster::measure::eAU;
    using cluster::measure::eLY;
    using cluster::measure::eMm;
    using cluster::measure::Format;
}
