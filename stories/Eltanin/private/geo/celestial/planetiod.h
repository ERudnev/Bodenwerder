#pragma once

#include <rmmr/math.q1.h>
#include <rmmr/system/core.q1.h>

#include <fQSM/api/interface.h>

#include <cstdint>

namespace eltanin::locality::geo {

    using namespace fqsm::api;

    struct Planetoid {
        struct Look {
            integer seed;
            float radius;
            float maxRelief;
            float surfaceAcceleration;
            float ridge;
        };

        struct Surface {
            float height;
            rmmr::vec3 position;
            rmmr::vec3 normal;
            std::uint64_t mix;
            float slope;
        };

        static auto placed() -> bool;
        static void place(Writing, rmmr::system::Device::Id, rmmr::Pose, Look);
        static void update(Writing, rmmr::Pos camera);

        static auto height(rmmr::vec3 dir) -> float;
        static auto altitudeAt(rmmr::Pos worldPos) -> float;
        static auto gravityAt(rmmr::Pos worldPos) -> rmmr::vec3;
        static auto surfaceInfo(rmmr::vec3 dir) -> Surface;
    };

}
