#pragma once

#include <rmmr/assets/semantics/geometry.h>
#include <rmmr/math.q1.h>
#include <rmmr/renderer/gl.q1.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/system/core.q1.h>

#include <GL/glew.h>

#include <fQSM/api/interface.h>

namespace rmmr::resource::geometry {

    using namespace fqsm::api;

    struct Asset : Feature<Asset, resource::Unit> {
        struct Channel {
            using Id = primitive::GeometrySemantics::PersistentId;
            using Type = primitive::GeometrySemantics::Type;
            using Layout = vector<Id>;
        };
        struct Quantum {
            Channel::Layout layout;
            vector<Pos> positions;
            vector<Pos> normals;
            vector<UV> uv0;
            vector<integer> indices;
        };
        struct Always {
            static auto layoutIds(const vector<string>& names) -> Channel::Layout;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Runtime : Entity<Runtime> {
        using VertexArray = GLuint;
        using VertexBuffer = GLuint;
        using ElementBuffer = GLuint;

        struct Quantum {
            system::Device::Id device;
            VertexArray vao;
            VertexBuffer vbo;
            ElementBuffer ebo;
            renderer::Count vertex_count;
            renderer::Count index_count;
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

    struct Composed : Feature<Composed, Asset> {
        struct Quantum {};
        struct Actions : BaseActions {
            static auto materialize(Writing, Id, system::Device::Id) -> Runtime::Quantum;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
