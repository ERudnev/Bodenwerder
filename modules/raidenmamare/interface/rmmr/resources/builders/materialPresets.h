#pragma once

#include <rmmr/resources/materials.q1.h>

namespace rmmr::resource::builders::material {

    using namespace fqsm::api;

    // Catalog recipes for material::Asset. Sampler slots are in uniforms; values come from the draw.
    struct Presets final {
        using Configured = resource::material::Asset::Quantum;
        static Configured ambient(resource::shader::Reference program, resource::shader::Reference shadow_depth);
        static Configured lit(resource::shader::Reference program, resource::shader::Reference shadow_depth);
        static Configured litTransparent(resource::shader::Reference program);
        static Configured litTextured(resource::shader::Reference program, resource::shader::Reference shadow_depth);
        static Configured litTexturedTransparent(resource::shader::Reference program);
        static Configured oneSidedGlass(resource::shader::Reference program);
        static Configured gizmoTextured(resource::shader::Reference program);
        static Configured gizmoVertexColor(resource::shader::Reference program);
        static Configured grid(resource::shader::Reference program);
        static Configured sprite(resource::shader::Reference program);
        static Configured identity(resource::shader::Reference program);
        static Configured familyGlow(resource::shader::Reference program);
    };

}
