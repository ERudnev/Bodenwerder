#pragma once

#include <base/maybe.h>

#include <rmmr/math.q1.h>
#include <rmmr/renderer/gl.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/shaders.q1.h>
#include <rmmr/resources/sprites.q1.h>
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
        mat4 model;
        resource::geometry::Runtime::Id geometry;
        resource::material::Runtime::Id material;
        resource::shader::Runtime::Id shader;
        base::maybe<resource::sprite::Runtime::Id> sprite;
        integer sprite_index;
        RGB albedo;
        float opacity;
        InstanceSource instance_data;
        Count instance_count;
        RenderState render_state;
    };

    using CommandBuffer = SeparateBuffers<Command>;

}
