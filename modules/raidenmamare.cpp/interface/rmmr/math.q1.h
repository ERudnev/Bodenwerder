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

}
