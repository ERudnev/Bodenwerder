#pragma once

#include <rmmr/resources/manager.q1.h>
#include <rmmr/semantics.q1.h>
#include <rmmr/resources/shaders.q1.h>
#include <rmmr/resources/textures.q1.h>
#include <rmmr/system/core.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::resource::material {

    using namespace fqsm::api;

    using Reference = resource::Unit::Reference;

    struct Asset : Feature<Asset, resource::Unit> {
        struct TextureBinding {
            Uniform::Id id;
            texture::Reference texture;
        };
        struct Technique {
            shader::Reference program;
            Uniform::Palette uniforms;
            vector<TextureBinding> textures;
        };
        struct Quantum {
            umap<renderer::Pass, Technique> techniques;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Runtime : Entity<Runtime> {
        using Locations = ::rmmr::material::Semantics::RuntimeMapping;
        struct TextureBinding {
            Uniform::Id id;
            texture::Runtime::Id texture;
        };
        struct Technique {
            shader::Runtime::Id shader;
            Locations locations;
            vector<Uniform::Binding> bindings;
            vector<TextureBinding> textures;
        };
        struct Quantum {
            umap<renderer::Pass, Technique> techniques;
        };
        struct Actions : BaseActions {
            static void apply(Reading, Id, system::Device::Id, renderer::Pass);
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Composer : Feature<Composer, Asset> {
        struct Quantum {};
        struct Actions : BaseActions {
            static auto materialize(Writing, Id, system::Device::Id) -> optional<Runtime::Id>;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
