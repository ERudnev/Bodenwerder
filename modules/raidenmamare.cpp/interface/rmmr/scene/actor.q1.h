#pragma once

#include <base/maybe.h>

#include <rmmr/renderer/types.q1.h>
#include <rmmr/resources_old/geometry.q1.h>
#include <rmmr/resources_old/material.q1.h>
#include <rmmr/scene/node.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::scene {

    using namespace fqsm::api;

    struct PrimitiveActor : Feature<PrimitiveActor, Node> {
        struct Quantum {
            resource_old::Geometry::Id geometry;
            resource_old::Material::Id material;
            RGB albedo;
        };
        struct Global {
            base::maybe<resource_old::Material::Id> shadowMaterial;
        };
        struct Actions : BaseActions {
            static auto create(Writing, Pos, HPB, resource_old::Geometry::Id, resource_old::Material::Id, RGB albedo) -> Id;
            static void submit(Reading, Id, renderer::CommandBuffer& where);
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
