#pragma once

#include <rmmr/resources/materials.q1.h>

namespace rmmr::resource::builders::material {

    using namespace fqsm::api;

    // Catalog recipes for material::Asset (not a resource kind — parallel to GeometryGenerator).
    struct Presets final {
        using Configured = resource::material::Asset::Quantum;
        static Configured ambient(resource::shader::Reference program, resource::shader::Reference shadow_depth);
        static Configured lit(resource::shader::Reference program, resource::shader::Reference shadow_depth);
        static Configured litTextured(resource::shader::Reference program, resource::texture::Reference albedo_map, resource::shader::Reference shadow_depth);
        static Configured litTexturedTransparent(resource::shader::Reference program, resource::texture::Reference albedo_map);
        static Configured grid(resource::shader::Reference program);
    };

}
