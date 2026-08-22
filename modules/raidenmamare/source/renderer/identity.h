#pragma once

#include "gl.h"

#include <rmmr/math.q1.h>
#include <rmmr/system/window.q1.h>
#include <rmmr/system/viewport.q1.h>

namespace rmmr {

    using namespace fqsm::api;

    struct Identity {
        renderer::Framebuffer allFbo;
        renderer::Framebuffer selectedFbo;
        renderer::Texture color;
        renderer::Texture selected;
        renderer::Texture depth;
        index2 size;

        void destroy();
        void ensure(index2 size);
        void clear(index2 size);
        void beginSelected(index2 size);
        void beginAll(index2 size);
        void end(Writing world, system::Viewport::Id viewport);
        auto peekUnder(Reading world, system::Window::Id window, system::Viewport::Id viewport, index2 viewportSize) -> renderer::Integer32;
    };

}
