#pragma once

#include "gl.h"

#include <rmmr/math.q1.h>
#include <rmmr/scene/root.q1.h>

#include <filesystem>

namespace rmmr {

    using namespace fqsm::api;

    struct FogPass {
        gl::ColorTarget target;
        renderer::Program program;

        void destroy();
        void ensurePrograms(const std::filesystem::path& shaders);
        auto apply(index2 size, const scene::Root::Quantum::Fog& fog, const mat4& invViewProj, vec3 cameraPos, renderer::Texture hdr, renderer::Texture depth, gl::Triangle& fullscreen) -> renderer::Texture;
    };

}
