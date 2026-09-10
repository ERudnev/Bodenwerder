#pragma once

namespace eltanin::locality::geo {

    struct Horizon {
        static constexpr float near = 5.0f;
        static constexpr float far = 100000.0f;
        static constexpr float locality = 80000.0f;
        static constexpr float systemInner = 90000.0f;
        static constexpr float systemOuter = 99000.0f;
        static constexpr float system = (systemInner + systemOuter) * 0.5f;
        static constexpr float stars = systemOuter;
        static constexpr float backdrop = 99500.0f;
        static constexpr float skyMesh = 100.0f;
    };

}
