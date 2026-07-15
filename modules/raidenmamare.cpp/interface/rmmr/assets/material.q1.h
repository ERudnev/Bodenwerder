#pragma once

#include <rmmr/assets/shader.q1.h>
#include <rmmr/assets/texture.q1.h>
#include <rmmr/resources_old/material.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::asset {

    using namespace fqsm::api;

    struct Material : Entity<Material> {
        struct TextureBinding {
            Uniform::Id id;
            Texture::Id texture;
        };
        struct Quantum {
            string name;
            Shader::Id program;
            Uniform::Palette uniforms;
            vector<TextureBinding> textures;
        };
        struct Always {
            static auto uniformIds(const vector<string>& names) -> Uniform::Palette;
        };
        struct Actions : BaseActions {
            static auto compile(Writing, Id, system::Device::Id) -> resource_old::Material::Id;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
