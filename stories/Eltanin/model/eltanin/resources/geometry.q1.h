#pragma once

#include <rmmr/resources/geometry.q1.h>
#include <rmmr/system/core.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::resource {

    using namespace fqsm::api;

    struct SkySphereGenerator : Feature<SkySphereGenerator, rmmr::resource::geometry::Asset> {
        struct Quantum {
            integer count = 48800;
            integer seed = 1;
            float angular_diameter_deg = 0.41f;
        };
        struct Actions : BaseActions {
            static auto materialize(Writing, Id, rmmr::system::Device::Id) -> optional<rmmr::resource::geometry::Runtime::Id>;
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

}
