#pragma once

#include <rmmr/math.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::geo {

    using namespace fqsm::api;

    struct Sun {
        struct Look {
            rmmr::HPB heading;
            rmmr::RGB color;
            float brightness;
            float angularDiameterDeg;
        };

        static auto sol() -> Look;
        static auto redGiant() -> Look;
        static auto placed() -> bool;
        static void place(Writing, Look);
        static void tether(Writing, rmmr::Pos camera);
    };

}
