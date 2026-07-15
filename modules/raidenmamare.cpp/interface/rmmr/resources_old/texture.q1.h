#pragma once

#include <rmmr/system/core.q1.h>

#include <GL/glew.h>

#include <fQSM/api/interface.h>

namespace rmmr::resource_old {

    using namespace fqsm::api;

    struct Texture : Entity<Texture> {
        using Handle = GLuint;

        struct Quantum {
            Handle handle;
            index2 size;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Texture_group : Group<Texture_group, system::Device, Texture> {
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
