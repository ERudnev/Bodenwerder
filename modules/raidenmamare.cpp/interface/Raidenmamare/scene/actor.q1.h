#pragma once

#include <Raidenmamare/resources/geometry.q1.h>
#include <Raidenmamare/resources/material.q1.h>
#include <Raidenmamare/scene/node.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::scene {

    using namespace fqsm::api;

    struct PrimitiveActor : Feature<PrimitiveActor, Node> {
        struct Quantum {
            resource::Geometry::Id geometry;
            resource::Material::Id material;
            RGB albedo;
        };
        struct Actions : BaseActions {
            static auto create(Writing, Pos, HPB, resource::Geometry::Id, resource::Material::Id, RGB albedo) -> Id;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
