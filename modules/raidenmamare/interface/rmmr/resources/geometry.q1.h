#pragma once

#include <rmmr/math.q1.h>
#include <rmmr/renderer/gl.q1.h>
#include <rmmr/resources/builders/geometryGenerator.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/system/core.q1.h>

#include <cstdint>

#include <fQSM/api/interface.h>

namespace rmmr::resource::geometry {

    using namespace fqsm::api;

    using Reference = resource::Unit::Reference;
    using EntryId = renderer::Integer32;
    using SurfaceId = renderer::Integer32;

    struct Runtime : Entity<Runtime> {
        struct Quantum {
            system::Device::Id device;
            renderer::VertexArray vao;
            renderer::VertexBuffer vbo;
            umap<primitive::GeometrySemantics::PersistentId, renderer::VertexBuffer> channels;
            renderer::ElementBuffer ebo;
            renderer::StorageBuffer primitiveSurfaces;
            renderer::Count vertex_count;
            renderer::Count index_count;
            vec3 boundMin;
            vec3 boundMax;
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

    struct Asset : Feature<Asset, resource::Unit> {
        struct Range {
            renderer::Count first;
            renderer::Count count;
        };
        struct Mount {
            string name;
            mat4 transform;
        };
        struct Entry {
            Range vertices;
            Range indices;
            Range surfaces;
            Range mounts;
            // LW layer pivot (etc.): verts baked relative to this; pose attaches here.
            vec3 origin;
        };
        struct Surface {
            Range indices;
        };
        struct Quantum {
            vector<Entry> entries;
            vector<Surface> surfaces;
            vector<Mount> mounts;
            umap<string, EntryId> entryCatalog;
            vector<umap<string, SurfaceId>> surfaceCatalogs;
        };
        struct Actions : BaseActions {
            // Bake CPU mesh into a Runtime and bind it in DeviceRuntimes mapping.
            static auto install(Writing, Id, system::Device::Id, const builders::geometry::CpuPresentation&) -> optional<Runtime::Id>;
            static auto install(Writing, Id, system::Device::Id, const builders::geometry::CpuPresentation&, const vector<SurfaceId>&, const umap<string, SurfaceId>&) -> optional<Runtime::Id>;
            static void writeChannel(Writing, Runtime::Id, primitive::GeometrySemantics::PersistentId, const void*, renderer::SizePtr);
            static void writeChannel(Stewarding, Runtime::Id, primitive::GeometrySemantics::PersistentId, const void*, renderer::SizePtr);
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Loader : Feature<Loader, Asset> {
        struct Quantum {
            filename file;
        };
        struct Actions : BaseActions {
            static auto materialize(Writing, Id, system::Device::Id) -> optional<Runtime::Id>;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Generator : Feature<Generator, Asset> {
        enum class Type : std::uint8_t {
            triangle,
            kube,
            bagel,
            gridPlane,
            unitQuad,
            sphere, // icosahedron; `subdivisions` frequency (1 = 80 tris)
            diamond, // regular octahedron; split verts; position+color0 (no UV/normals)
        };
        struct Quantum {
            Type type;
            integer subdivisions;
        };
        struct Actions : BaseActions {
            static auto materialize(Writing, Id, system::Device::Id) -> optional<Runtime::Id>;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
