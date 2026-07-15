#pragma once

#include <rmmr/math.q1.h>
#include <rmmr/renderer/types.q1.h>
#include <rmmr/resources_old/geometry.q1.h>
#include <rmmr/resources_old/material.q1.h>
#include <rmmr/scene/node.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::scene {

    using namespace fqsm::api;

    struct Grid : Feature<Grid, Node> {
        struct Quantum {
            resource_old::Geometry::Id geometry;
            resource_old::Material::Id material;
            float opacity;
        };
        struct Actions : BaseActions {
            static auto create(Writing, Pos, HPB, Quantum) -> Id;
            static void submit(Reading, Id, renderer::CommandBuffer& where);
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
