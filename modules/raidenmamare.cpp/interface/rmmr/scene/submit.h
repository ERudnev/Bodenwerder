#pragma once

#include <rmmr/renderer/types.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/system/core.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::scene {

    using namespace fqsm::api;

    // Instance + assets; passes come from material.techniques keys.
    struct DrawInstance {
        mat4 model;
        resource::geometry::Asset::Id geometry;
        resource::material::Asset::Id material;
        RGB albedo;
        float opacity;
    };

    void submit_material_passes(Reading, system::Device::Id, const DrawInstance&, renderer::CommandBuffer& where);

}
