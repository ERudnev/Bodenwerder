#pragma once

#include <base/maybe.h>
#include <rmmr/math.q1.h>
#include <rmmr/renderer/types.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/sprites.q1.h>
#include <rmmr/scene/node.q1.h>
#include <rmmr/system/core.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::scene::actor {

    using namespace fqsm::api;

    // Sprite draw in scene space. Rotation from Node; local scale here.
    // tint: relative additive RGB (zero = neutral). vec3 allows negatives.
    struct Sprite : Feature<Sprite, Node> {
        struct Quantum {
            resource::material::Asset::Id material;
            RGB tint;
            vec3 scale;
            resource::sprite::Pack::Id pack;
            integer index;
        };
        struct Global {
            base::maybe<resource::geometry::Asset::Id> geometry;
        };
        struct Actions : BaseActions {
            static auto create(Writing, Pos, HPB, vec3 scale, resource::material::Asset::Id, RGB tint, resource::sprite::Pack::Id, integer index) -> Id;
            static void submit(Reading, Id, system::Device::Id, renderer::CommandBuffer& where);
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
