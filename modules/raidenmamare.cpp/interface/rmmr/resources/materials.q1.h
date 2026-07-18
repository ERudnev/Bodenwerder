#pragma once

#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/semantics.q1.h>
#include <rmmr/resources/shaders.q1.h>
#include <rmmr/resources/textures.q1.h>
#include <rmmr/system/core.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::resource::material {

    using namespace fqsm::api;

    struct Asset : Feature<Asset, resource::Unit> {
        struct TextureBinding {
            Uniform::Id id;
            texture::Asset::Id texture;
        };
        struct Quantum {
            shader::Asset::Id program;
            Uniform::Palette uniforms;
            vector<TextureBinding> textures;
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
        struct Quantum {
            shader::Runtime::Id shader;
            Locations locations;
            vector<Uniform::Binding> bindings;
            vector<TextureBinding> textures;
        };
        struct Actions : BaseActions {
            static void apply(Reading, Id, system::Device::Id);
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
