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
        struct Quantum {
            system::Device::Id device;
            renderer::Program handle;
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

    struct Loader : Feature<Loader, Asset> {
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
