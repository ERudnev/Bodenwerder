#pragma once

#include <rmmr/renderer/types.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/scene/node.q1.h>
#include <rmmr/system/core.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::scene {

    using namespace fqsm::api;

    struct PrimitiveActor : Feature<PrimitiveActor, Node> {
        struct Quantum {
            resource::geometry::Asset::Id geometry;
            resource::material::Asset::Id material;
            RGB albedo;
        };
        struct Actions : BaseActions {
            static auto create(Writing, Pos, HPB, resource::geometry::Asset::Id, resource::material::Asset::Id, RGB albedo) -> Id;
            static void submit(Reading, Id, system::Device::Id, renderer::CommandBuffer& where);
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
