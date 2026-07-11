#pragma once

#include <Raidenmamare/system/core.q1.h>

#include <GL/glew.h>

#include <fQSM/api/interface.h>

namespace rmmr::resource {

    using namespace fqsm::api;

    struct Geometry : Entity<Geometry> {
        using VertexArray = GLuint;
        using VertexBuffer = GLuint;
        using ElementBuffer = GLuint;

        struct Quantum {
            VertexArray vao;
            VertexBuffer vbo;
            ElementBuffer ebo;
            integer vertex_count;
            integer index_count;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Geometry_group : Group<Geometry_group, system::Device, Geometry> {
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
