#pragma once

#include "gl.h"
#include "identity.h"

#include <rmmr/math.q1.h>
#include <rmmr/resources/overlays.q1.h>
#include <rmmr/system/window.q1.h>
#include <rmmr/system/viewport.q1.h>

#include <filesystem>
#include <span>

namespace rmmr {

    using namespace fqsm::api;

    struct OverlayCompose {
        gl::ColorTarget sceneColor;
        gl::ColorTarget overlayColor;
        renderer::Program composeProgram;

        void destroy();
        void ensurePrograms(const std::filesystem::path& shaders);
        void captureWindow(index2 size);
        void run(Writing world, system::Window::Id window, system::Viewport::Id viewport, resource::overlay::Asset::Id overlay, std::span<const renderer::Integer32> selection, const Identity& identity, gl::Triangle& fullscreen, index2 size);
        void compose(index2 size, gl::Triangle& fullscreen);
    };

}
