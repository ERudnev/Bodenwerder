#pragma once

#include <rmmr/math.q1.h>
#include <rmmr/renderer/gl.q1.h>
#include <rmmr/resources_old/geometry.q1.h>
#include <rmmr/resources_old/material.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::renderer {

    using namespace fqsm::api;

    using InstanceBuffer = resource_old::Geometry::VertexBuffer;

    enum class Pass {
        opaque,
        transparent,
        shadow,
        ui,
        gizmo,
    };

    struct RenderState {
    };

    struct InstanceSource {
        InstanceBuffer buffer;
        IntPtr byte_offset;
    };

    struct Command {
        Pass pass;
        mat4 model;
        resource_old::Geometry::Id geometry;
        resource_old::Material::Id material;
        RGB albedo;
        float opacity;
        InstanceSource instance_data;
        Count instance_count;
        RenderState render_state;
    };

    using CommandBuffer = vector<Command>;

}
