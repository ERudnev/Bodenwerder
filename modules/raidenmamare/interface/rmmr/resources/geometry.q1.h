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

    struct Runtime : Entity<Runtime> {
        struct Quantum {
            system::Device::Id device;
            renderer::VertexArray vao;
            renderer::VertexBuffer vbo;
            renderer::ElementBuffer ebo;
            renderer::Count vertex_count;
            renderer::Count index_count;
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

    struct Asset : Feature<Asset, resource::Unit> {
        struct Part {
            renderer::Count startIndex;
            renderer::Count countIndex;
        };
        struct Quantum {
            vector<Pos> slots;
            umap<string, Part> parts;
        };
        struct Actions : BaseActions {
            // Bake CPU mesh into a Runtime and bind it in DeviceRuntimes mapping.
            static auto install(Writing, Id, system::Device::Id, const builders::geometry::CpuPresentation&) -> optional<Runtime::Id>;
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
            windowedKube,
            bagel,
            gridPlane,
            unitQuad,
            sphere, // regular icosahedron (20 faces); smooth sphere normals + spherical UV
            diamond, // regular octahedron; split verts; position+color0 (no UV/normals)
        };
        struct Quantum {
            Type type;
        };
        struct Actions : BaseActions {
            static auto materialize(Writing, Id, system::Device::Id) -> optional<Runtime::Id>;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
