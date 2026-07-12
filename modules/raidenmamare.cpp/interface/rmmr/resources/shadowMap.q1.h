#pragma once

#include <rmmr/system/core.q1.h>

#include <GL/glew.h>

#include <fQSM/api/interface.h>

namespace rmmr::resource {

    using namespace fqsm::api;

    struct ShadowMap : Entity<ShadowMap> {
        using Framebuffer = GLuint;
        using DepthTexture = GLuint;

        struct Quantum {
            Framebuffer fbo;
            DepthTexture depth;
            index2 size;
        };
        struct Actions : BaseActions {
            static void bind(Reading, Id);
            static void clear(Reading, Id);
            static void unbind(Reading, Id);
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct ShadowMap_group : Group<ShadowMap_group, system::Device, ShadowMap> {
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
