#pragma once

#include <rmmr/renderer/gl.q1.h>
#include <rmmr/resources/manager.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::resource::texture3array {

    using namespace fqsm::api;

    using Reference = resource::Unit::Reference;
    using LayerRgba = vector<unsigned char>;

    struct CpuPresentation {
        index3 layerSize;
        vector<LayerRgba> layers;
    };

    struct Runtime : Entity<Runtime> {
        struct Quantum {
            system::Device::Id device;
            vector<renderer::Texture> layers;
            index3 layerSize;
            integer capacity;
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

    struct Asset : Feature<Asset, resource::Unit> {
        struct Quantum {
            index3 layerSize;
            integer capacity;
        };
        struct Actions : BaseActions {
            static auto install(Writing, Id, system::Device::Id, const CpuPresentation& cpu) -> optional<Runtime::Id>;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
