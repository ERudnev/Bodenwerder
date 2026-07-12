#pragma once

#include <rmmr/system/core.q1.h>

#include <GL/glew.h>

#include <fQSM/api/interface.h>

namespace rmmr::resource {

    using namespace fqsm::api;

    struct Shader : Entity<Shader> {
        using Handle = GLuint;

        struct Quantum {
            Handle handle;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Shader_group : Group<Shader_group, system::Device, Shader> {
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
