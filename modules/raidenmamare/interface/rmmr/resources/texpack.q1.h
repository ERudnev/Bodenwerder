#pragma once

#include <rmmr/renderer/gl.q1.h>
#include <rmmr/resources/manager.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::resource::texpack {

    using namespace fqsm::api;

    using Reference = resource::Unit::Reference;

    struct Runtime : Entity<Runtime> {
        struct Quantum {
            system::Device::Id device;
            renderer::Texture handle;
            index2 layerSize;
            integer capacity;
            umap<string, integer> layers;
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

    struct Pack : Feature<Pack, resource::Unit> {
        struct Quantum {
            index2 layerSize;
            integer capacity;
            vector<string> layers;
        };
        struct Actions : BaseActions {
            static auto materialize(Writing, Id, system::Device::Id) -> optional<Runtime::Id>;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct LoaderCatalog : Feature<LoaderCatalog, Pack> {
        struct Quantum {
            filename directory;
        };
        struct Actions : BaseActions {
            static void load(Writing, Id);
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
