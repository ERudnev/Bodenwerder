#pragma once

#include <Raidenmamare/assets/shader.q1.h>
#include <Raidenmamare/resources/material.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::asset {

    using namespace fqsm::api;

    struct Material : Entity<Material> {
        struct Quantum {
            string name;
            Shader::Id program;
            Uniform::Palette uniforms;
        };
        struct Always {
            static auto uniformIds(const vector<string>& names) -> Uniform::Palette;
        };
        struct Actions : BaseActions {
            static auto compile(Writing, Id, system::Device::Id) -> resource::Material::Id;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
