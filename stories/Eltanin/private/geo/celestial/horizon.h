#pragma once

#include "cluster/measure.h"

namespace eltanin::geo {

    struct Horizon {
        static constexpr float near = 2.0f;
        static constexpr float far = float(2000.0 * eMm);
        static constexpr float locality = float(1800.0 * eMm);
        static constexpr float systemInner = float(1900.0 * eMm);
        static constexpr float systemOuter = float(1990.0 * eMm);
        static constexpr float system = (systemInner + systemOuter) * 0.5f;
        static constexpr float stars = systemOuter;
        static constexpr float backdrop = float(1995.0 * eMm);
        static constexpr float skyMesh = 100.0f;
    };

}
