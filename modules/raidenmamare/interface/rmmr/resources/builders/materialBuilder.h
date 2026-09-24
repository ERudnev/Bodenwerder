#pragma once

#include <base/maybe.h>
#include <fQSM/api/interface.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/shaders.q1.h>

namespace rmmr::resource::builders::material {

    using namespace fqsm::api;

    struct SinglePass {
        Unit::Name name;
        shader::Loader::Quantum shader;
        renderer::Pass pass;
        Uniform::Palette uniforms;
        bool glowSpread;
        renderer::LightingMode lighting;
        bool nearest;
        renderer::RenderState renderState;
    };

    struct Derived {
        Unit::Name name;
        resource::material::Asset::Id source;
        renderer::Pass sourcePass;
        renderer::Pass targetPass;
        base::maybe<shader::Reference> program;
        bool glowSpread;
        base::maybe<renderer::RenderState> renderState;
    };

    auto addSinglePass(Writing, SinglePass) -> resource::material::Asset::Id;
    auto derive(Writing, Derived) -> base::maybe<resource::material::Asset::Id>;

}
