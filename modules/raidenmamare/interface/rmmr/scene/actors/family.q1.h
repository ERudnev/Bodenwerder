#pragma once

#include <base/maybe.h>
#include <rmmr/math.q1.h>
#include <rmmr/renderer/types.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/meshpack.q1.h>
#include <rmmr/resources/texpack.q1.h>
#include <rmmr/scene/node.q1.h>
#include <rmmr/semantics.q1.h>
#include <rmmr/system/core.q1.h>

#include <fQSM/api/interface.h>

#include <cstddef>
#include <vector>

namespace rmmr::scene::actor {

    using namespace fqsm::api;

    using Packed = std::vector<std::byte>;

    struct Family : Entity<Family> {
        struct Field {
            string name;
            resource::Uniform::Type type;
            integer offset;
        };
        struct Layout {
            integer instanceBytes;
            vector<Field> fields;
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
            Layout layout;
            renderer::StorageBuffer actorState;
            renderer::StorageBuffer instances;
            renderer::StorageBuffer drawMetadata;
            renderer::StorageBuffer surfacePalette;
            vector<Bucket> buckets;
        };
        struct Actions : BaseActions {
            static auto field(Reading, Id, string) -> optional<Field>;
            static void write(Reading, Id, Packed&, string, float);
            static void write(Reading, Id, Packed&, string, integer);
            static void write(Reading, Id, Packed&, string, vec2);
            static void write(Reading, Id, Packed&, string, vec3);
            static void submit(Reading, Id, system::Device::Id, renderer::CommandBuffer& where);
            static auto compose(Reading, resource::meshpack::Asset::Resolved, Layout) -> optional<Quantum>;
            static auto composeOne(Reading, resource::geometry::Asset::Id, resource::material::Asset::Id, Layout) -> optional<Quantum>;
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

    struct Replica : Feature<Replica, Node> {
        struct Quantum {
            Family::Id family;
            Packed packed;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Replica_group : Group<Replica_group, Family, Replica> {
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
