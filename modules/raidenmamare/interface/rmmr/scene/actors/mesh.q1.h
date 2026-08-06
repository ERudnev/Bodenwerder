#pragma once

#include <base/maybe.h>
#include <rmmr/renderer/types.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/scene/node.q1.h>
#include <rmmr/system/core.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::scene::actor {

    using namespace fqsm::api;

    struct Mesh : Feature<Mesh, Node> {
        struct Quantum {
            resource::geometry::Asset::Id geometry;
            umap<string, resource::material::Asset::Id> materials;
            RGB albedo;
            vec3 scale;
            float opacity;
            bool visible;
        };
        struct Actions : BaseActions {
            static auto create(Writing, Pos, HPB, resource::geometry::Asset::Id, umap<string, resource::material::Asset::Id>, RGB albedo) -> Id;
            static void setVisible(Writing, Id, bool);
            static void submit(Reading, Id, system::Device::Id, renderer::CommandBuffer& where);
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    // Opt-in pick id on a Mesh (same Node id). GPU writes scenicAlias into Pass::identity
    // (+ Pass::identitySelected when selected).
    struct Identified : Feature<Identified, Mesh> {
        struct Quantum {
            renderer::Integer32 scenicAlias;
            bool selected;
        };
        struct Global {
            integer lastGeneratedId;
            base::maybe<resource::material::Asset::Id> material;
        };
        struct Actions : BaseActions {
            static void extend(Writing, Mesh::Id);
            static auto lookup(Reading, renderer::Integer32) -> optional<Id>;
            static void applySelection(Writing, const vector<renderer::Integer32>&);
            static void submit(Reading, Id, system::Device::Id, renderer::CommandBuffer& where);
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
