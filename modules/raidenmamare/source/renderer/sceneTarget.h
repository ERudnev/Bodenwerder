#pragma once

#include "gl.h"

#include <rmmr/math.q1.h>

namespace rmmr {

    using namespace fqsm::api;

    struct SceneTarget {
        renderer::Framebuffer fbo;
        renderer::Texture hdr;
        renderer::Texture bloomMask;
        renderer::Texture depth;
        index2 size;

        void destroy();
        void ensure(index2 size);
        void bind(index2 size);
        void begin(index2 size, vec4 clearColor);
        static void setGlowWrite(bool on);
    };

}
