#pragma once

namespace eltanin::geo {

    struct Horizon {
        static constexpr float near = 2.0f;
        static constexpr float far = 20000000.0f;
        static constexpr float locality = 18000000.0f;
        static constexpr float systemInner = 19000000.0f;
        static constexpr float systemOuter = 19900000.0f;
        static constexpr float system = (systemInner + systemOuter) * 0.5f;
        static constexpr float stars = systemOuter;
        static constexpr float backdrop = 19950000.0f;
        static constexpr float skyMesh = 100.0f;
    };

}
