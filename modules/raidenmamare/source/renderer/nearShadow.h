#pragma once

#include "gl.h"
#include "nearShadowMath.h"

namespace rmmr {

    struct NearShadow {
        struct Format {
            int resolution;
            unsigned stateBinding;
            unsigned textureUnit;
        };

        Format format;
        renderer::Framebuffer fbo;
        renderer::Texture depth;
        renderer::UniformBuffer stateBuffer;
        NearShadowFrame frame;

        void prepare(const NearShadowFrame& next);
        void begin();
        void bind();
        void destroy();
    };

}
