#pragma once

#include <rmmr/math.q1.h>
#include <rmmr/renderer/gl.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/semantics/rendering.h>

#include <fQSM/api/interface.h>

namespace rmmr::renderer {

    using namespace fqsm::api;

    struct RenderState {
    };

    struct InstanceSource {
        VertexBuffer buffer;
        IntPtr byte_offset;
    };

    struct Command {
        Pass pass;
        mat4 model;
        resource::geometry::Runtime::Id geometry;
        resource::material::Runtime::Id material;
        RGB albedo;
        float opacity;
        InstanceSource instance_data;
        Count instance_count;
        RenderState render_state;
    };

    using CommandBuffer = vector<Command>;

}
