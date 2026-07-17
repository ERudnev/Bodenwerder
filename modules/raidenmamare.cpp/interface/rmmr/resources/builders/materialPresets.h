#pragma once

#include <rmmr/resources/materials.q1.h>

namespace rmmr::resource::builders::material {

    using namespace fqsm::api;

    // Catalog recipes for material::Asset (not a resource kind — parallel to GeometryGenerator).
    struct MaterialPresets final {
        using Configured = resource::material::Asset::Quantum;
        static Configured ambient(resource::shader::Asset::Id program);
        static Configured lit(resource::shader::Asset::Id program);
        static Configured litTextured(resource::shader::Asset::Id program, resource::texture::Asset::Id albedo_map);
        static Configured grid(resource::shader::Asset::Id program);
        static Configured shadowDepth(resource::shader::Asset::Id program);
    };

}
