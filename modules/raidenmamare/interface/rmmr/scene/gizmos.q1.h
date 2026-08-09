#pragma once

#include <rmmr/math.q1.h>
#include <rmmr/renderer/types.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/system/core.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::scene {

    using namespace fqsm::api;

    struct Grid : Feature<Grid, actor::Mesh> {
        struct Quantum {
            resource::geometry::Asset::Id geometry;
            resource::material::Asset::Id material;
            float opacity;
            float patternScale;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
