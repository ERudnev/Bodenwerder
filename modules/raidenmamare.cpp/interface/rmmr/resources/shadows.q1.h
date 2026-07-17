#pragma once

#include <rmmr/renderer/gl.q1.h>
#include <rmmr/resources/manager.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::resource::shadow {

    using namespace fqsm::api;

    struct Asset : Feature<Asset, resource::Unit> {
        struct Quantum {};
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Runtime : Entity<Runtime> {
        using Framebuffer = GLuint;
        using DepthTexture = GLuint;

        struct Quantum {
            system::Device::Id device;
            Framebuffer fbo;
            DepthTexture depth;
            index2 size;
        };
        struct Actions : BaseActions {
            static void bind(Reading, Id);
            static void clear(Reading, Id);
            static void unbind(Reading, Id);
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

    struct Allocator : Feature<Allocator, Asset> {
        struct Quantum {
            index2 size;
        };
        struct Actions : BaseActions {
            static auto materialize(Writing, Id, system::Device::Id) -> Runtime::Quantum;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
