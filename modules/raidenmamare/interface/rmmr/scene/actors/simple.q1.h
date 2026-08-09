#pragma once

#include <base/maybe.h>
#include <rmmr/renderer/types.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/texpack.q1.h>
#include <rmmr/scene/node.q1.h>
#include <rmmr/system/core.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::scene::actor {

    using namespace fqsm::api;

    struct Simple : Feature<Simple, Node> {
        struct Quantum {
            resource::geometry::Asset::Id geometry;
            resource::material::Asset::Id material;
            base::maybe<resource::texpack::Pack::Id> texpack;
            base::maybe<string> albedoLayer;
            RGB albedo;
            vec3 scale;
        };
        struct Actions : BaseActions {
            static auto create(Writing, Pos, HPB, resource::geometry::Asset::Id, resource::material::Asset::Id, base::maybe<resource::texpack::Pack::Id>, base::maybe<string>, RGB albedo) -> Id;
            static void submit(Reading, Id, system::Device::Id, renderer::CommandBuffer& where);
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
