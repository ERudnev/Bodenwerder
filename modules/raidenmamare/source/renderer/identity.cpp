#include "identity.h"

#include <algorithm>
#include <cmath>

#include <rmmr/system/core.q1.h>
#include <imgui.h>
#include <GLFW/glfw3.h>

namespace rmmr {

    using namespace gl;

    void Identity::destroy() {
        releaseFramebuffer(allFbo);
        releaseFramebuffer(selectedFbo);
        releaseTexture(color);
        releaseTexture(selected);
        releaseTexture(depth);
        size = index2{0, 0};
    }

    void Identity::ensure(index2 targetSize) {
        if (allFbo and selectedFbo and size.x == targetSize.x and size.y == targetSize.y)
            return;
        destroy();
        color = makeTexture2D(targetSize, GL_R32UI, GL_NEAREST, GL_NEAREST);
        selected = makeTexture2D(targetSize, GL_R32UI, GL_NEAREST, GL_NEAREST);
        depth = makeTexture2D(targetSize, GL_DEPTH_COMPONENT24, GL_NEAREST, GL_NEAREST);
        allFbo = makeFramebuffer();
        attachColor(allFbo, 0, color);
        attachDepth(allFbo, depth);
        finishFramebuffer(allFbo, 1, "identity");
        selectedFbo = makeFramebuffer();
        attachColor(selectedFbo, 0, selected);
        attachDepth(selectedFbo, depth);
        finishFramebuffer(selectedFbo, 1, "identitySelected");
        size = targetSize;
    }

    void Identity::clear(index2 size) {
        ensure(size);
        const auto wh = extent(size);
        glViewport(0, 0, wh.x, wh.y);
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_TRUE);
        const GLuint clearAlias[]{0u};
        glBindFramebuffer(GL_FRAMEBUFFER, selectedFbo);
        glClearBufferuiv(GL_COLOR, 0, clearAlias);
        glClear(GL_DEPTH_BUFFER_BIT);
        glBindFramebuffer(GL_FRAMEBUFFER, allFbo);
        glClearBufferuiv(GL_COLOR, 0, clearAlias);
    }

    void Identity::beginSelected(index2 size) {
        clear(size);
        const auto wh = extent(size);
        glBindFramebuffer(GL_FRAMEBUFFER, selectedFbo);
        glViewport(0, 0, wh.x, wh.y);
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_TRUE);
    }

    void Identity::beginAll(index2 size) {
        ensure(size);
        const auto wh = extent(size);
        glBindFramebuffer(GL_FRAMEBUFFER, allFbo);
        glViewport(0, 0, wh.x, wh.y);
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_TRUE);
    }

    void Identity::end(Writing world, system::Viewport::Id viewport) {
        glDepthFunc(GL_LESS);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        system::Viewport::Actions::activate(world, viewport);
    }

    auto Identity::peekUnder(Reading world, system::Window::Id window, system::Viewport::Id viewport, index2 viewportSize) -> renderer::Integer32 {
        if (ImGui::GetIO().WantCaptureMouse)
            return renderer::Integer32{0};

        const auto& windowState = with<system::Window>::get(world, window);
        const auto& viewportState = with<system::Viewport>::get(world, viewport);
        const auto fb = system::Window::Actions::framebufferSize(world, window);

        int winW = 1;
        int winH = 1;
        glfwGetWindowSize(with<system::Device>::get(world, window).handle, &winW, &winH);
        winW = std::max(winW, 1);
        winH = std::max(winH, 1);

        const auto fbX = static_cast<integer>(std::lround(static_cast<double>(windowState.current.mouse.x) * static_cast<double>(fb.x) / static_cast<double>(winW)));
        const auto fbY = static_cast<integer>(std::lround(static_cast<double>(windowState.current.mouse.y) * static_cast<double>(fb.y) / static_cast<double>(winH)));
        const auto localX = fbX - viewportState.origin.x;
        const auto localY = fbY - viewportState.origin.y;
        if (localX < 0 or localY < 0 or localX >= viewportSize.x or localY >= viewportSize.y)
            return renderer::Integer32{0};

        GLuint alias = 0;
        glReadPixels(static_cast<int>(localX), static_cast<int>(viewportSize.y) - 1 - static_cast<int>(localY), 1, 1, GL_RED_INTEGER, GL_UNSIGNED_INT, &alias);
        return static_cast<renderer::Integer32>(alias);
    }

}
