#pragma once

#include "gl.h"

#include <rmmr/math.q1.h>
#include <rmmr/system/viewport.q1.h>

#include <filesystem>

namespace rmmr {

    using namespace fqsm::api;

    struct Bloom {
        renderer::Framebuffer sourceFbo;
        renderer::Framebuffer scratchFbo;
        renderer::Texture source;
        renderer::Texture scratch;
        index2 size;
        renderer::Program downsampleProgram;
        renderer::Program blurProgram;
        renderer::Program tonemapProgram;

        void destroy();
        void ensure(index2 sceneSize);
        void ensurePrograms(const std::filesystem::path& shaders);
        void downsample(renderer::Texture hdr, renderer::Texture bloomMask, gl::Triangle& fullscreen);
        void blur(float radius, gl::Triangle& fullscreen);
        void tonemapToWindow(Writing world, system::Viewport::Id viewport, renderer::Texture hdr, float intensity, gl::Triangle& fullscreen);
    };

}
