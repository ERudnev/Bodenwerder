#pragma once

namespace eltanin::locality::geo {

    struct Horizon {
        static constexpr float near = 2.0f;
        static constexpr float far = 200000.0f;
        static constexpr float locality = 180000.0f;
        static constexpr float systemInner = 190000.0f;
        static constexpr float systemOuter = 199000.0f;
        static constexpr float system = (systemInner + systemOuter) * 0.5f;
        static constexpr float stars = systemOuter;
        static constexpr float backdrop = 199500.0f;
        static constexpr float skyMesh = 100.0f;
    };

}
