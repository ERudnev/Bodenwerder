#pragma once

#include <rmmr/assets/shader.q1.h>
#include <rmmr/assets/semantics/uniform.h>
#include <rmmr/resources/shader.q1.h>
#include <rmmr/resources/texture.q1.h>
#include <rmmr/system/core.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::resource {

    using namespace fqsm::api;

    struct Material : Entity<Material> {
        using Locations = material::Semantics::RuntimeMapping;
        struct TextureBinding {
            asset::Uniform::Id id;
            Texture::Id texture;
        };

        struct Quantum {
            Shader::Id shader;
            Locations locations;
            vector<asset::Uniform::Binding> bindings;
            vector<TextureBinding> textures;
        };
        struct Actions : BaseActions {
            static void apply(Reading, Id, system::Device::Id);
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Material_group : Group<Material_group, system::Device, Material> {
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
