#pragma once

#include <rmmr/renderer/gl.q1.h>
#include <rmmr/resources/manager.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::resource::texture {

    using namespace fqsm::api;

    struct Asset : Feature<Asset, resource::Unit> {
        struct Quantum {};
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Runtime : Entity<Runtime> {
        struct Quantum {
            system::Device::Id device;
            renderer::Texture handle;
            index2 size;
        };
        struct Internals;
        static const Behavior customAspectReactions();
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
        struct Quantum {
            index2 size;
        };
        struct Actions : BaseActions {
            static auto materialize(Writing, Id, system::Device::Id) -> optional<Runtime::Id>;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
