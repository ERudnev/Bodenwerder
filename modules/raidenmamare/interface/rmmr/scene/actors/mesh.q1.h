#pragma once

#include <rmmr/renderer/types.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/meshpack.q1.h>
#include <rmmr/resources/sprites.q1.h>
#include <rmmr/resources/texpack.q1.h>
#include <rmmr/scene/node.q1.h>
#include <rmmr/system/core.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::scene::actor {

    using namespace fqsm::api;

    struct Mesh : Feature<Mesh, Node> {
        struct Occurrence {
            resource::meshpack::Asset::Resolved entry;
            renderer::DiscretePose pose;
        };
        struct Bucket {
            resource::geometry::Runtime::Id geometry;
            resource::material::Runtime::Id material;
            base::maybe<resource::texpack::Runtime::Id> texpack;
            renderer::IndirectBuffer indirect;
            renderer::Count drawCount;
            renderer::IntPtr metadataByteOffset;
            renderer::SizePtr metadataByteSize;
        };
        struct Quantum {
            system::Device::Id device;
            renderer::StorageBuffer actorState;
            renderer::StorageBuffer poses;
            renderer::StorageBuffer drawMetadata;
            renderer::StorageBuffer surfacePalette;
            base::maybe<resource::sprite::Runtime::Id> sprite;
            integer spriteIndex;
            vector<Bucket> buckets;
        };
        struct Actions : BaseActions {
            static auto compose(Reading, const vector<Occurrence>&) -> optional<Quantum>;
            static auto compose(Reading, resource::meshpack::Asset::Resolved) -> optional<Quantum>;
            static auto composeOne(Reading, resource::geometry::Asset::Id, resource::material::Asset::Id) -> optional<Quantum>;
            static void submit(Reading, Id, system::Device::Id, renderer::CommandBuffer& where);
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

    struct MeshState : Feature<MeshState, Mesh> {
        struct Quantum {
            RGB albedo;
            vec3 scale;
            float latticeStep;
            float patternScale;
            float opacity;
            bool visible;
        };
        struct Actions : BaseActions {
            static void setVisible(Writing, Id, bool);
            static auto defaults() -> Quantum;
            static auto defaults(RGB albedo, float opacity) -> Quantum;
            static auto defaults(RGB albedo, float opacity, vec3 scale) -> Quantum;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

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
