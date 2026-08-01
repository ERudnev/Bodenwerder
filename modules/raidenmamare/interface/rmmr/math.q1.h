#pragma once

#include <fQSM/api/interface.h>

namespace rmmr {

    using namespace fqsm::api;

    using Pos = vec3;
    using HPB = vec3;
    using RGB = vec3;
    using UV = vec2;

    struct Locator {
        Pos pos;
        HPB euler;
    };

    struct Pose {
        Pos position;
        quat rotation;

        auto hpb() const -> HPB;
        void hpb(HPB);
        auto near(const Pose&) const -> bool;
        static auto from(Locator) -> Pose;
    };

}
