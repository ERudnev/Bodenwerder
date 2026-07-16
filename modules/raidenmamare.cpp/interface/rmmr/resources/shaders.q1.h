#pragma once

#include <rmmr/renderer/gl.q1.h>
#include <rmmr/resources/manager.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::resource::shader {

    using namespace fqsm::api;

    struct Asset : Feature<Asset, resource::Unit> {
        struct Quantum {};
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Runtime : Entity<Runtime> {
        using Handle = GLuint;

        struct Quantum {
            system::Device::Id device;
            Handle handle;
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

    struct FromFile : Feature<FromFile, Asset> {
        struct Quantum {
            filename vertex;
            filename fragment;
        };
        struct Actions : BaseActions {
            static auto materialize(Writing, Id, system::Device::Id) -> Runtime::Quantum;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
