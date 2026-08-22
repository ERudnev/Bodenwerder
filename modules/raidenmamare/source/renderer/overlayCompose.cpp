#include "overlayCompose.h"

#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/resources/shaders.q1.h>
#include <rmmr/semantics.q1.h>

#include <algorithm>
#include <GL/glew.h>

namespace rmmr {

    using namespace gl;

    void OverlayCompose::destroy() {
        gl::destroy(sceneColor);
        gl::destroy(overlayColor);
        releaseProgram(composeProgram);
    }

    void OverlayCompose::ensurePrograms(const std::filesystem::path& shaders) {
        if (not composeProgram)
            composeProgram = compileProgramFromFiles(shaders / "fullscreen.vert.glsl", shaders / "overlayCompose.frag.glsl", "overlay compose");
    }

    void OverlayCompose::captureWindow(index2 size) {
        ensureColorTarget(sceneColor, size, GL_RGBA8, GL_LINEAR, "sceneColor");
        const auto wh = extent(size);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, sceneColor.fbo);
        glBlitFramebuffer(0, 0, wh.x, wh.y, 0, 0, wh.x, wh.y, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void OverlayCompose::run(Writing world, system::Window::Id window, system::Viewport::Id viewport, resource::overlay::Asset::Id overlay, std::span<const renderer::Integer32> selection, const Identity& identity, gl::Triangle& fullscreen, index2 size) {
        const auto& runtimes = with<resource::Runtimes>::get(world, window);
        const auto overlayIt = runtimes.overlays_id_mapping.find(overlay);
        if (overlayIt == runtimes.overlays_id_mapping.end())
            return;

        const auto& overlayRuntime = with<resource::overlay::Runtime>::get(world, overlayIt->second);
        const auto& shader = with<resource::shader::Runtime>::get(world, overlayRuntime.shader);
        const int divisor = resource::overlay::scale_divisor(overlayRuntime.scale);
        const index2 effectSize{std::max(static_cast<int>(size.x) / divisor, 1), std::max(static_cast<int>(size.y) / divisor, 1)};
        ensureColorTarget(overlayColor, effectSize, GL_RGBA8, GL_LINEAR, "overlay");
        const auto wh = extent(effectSize);

        glBindFramebuffer(GL_FRAMEBUFFER, overlayColor.fbo);
        glViewport(0, 0, wh.x, wh.y);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glDisable(GL_BLEND);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(shader.handle);

        const auto idScene = material::Semantics::id_of("sceneColor");
        const auto idIdent = material::Semantics::id_of("identiffyMap");
        const auto idSelectedMap = material::Semantics::id_of("selectedMap");
        const auto idTexel = material::Semantics::id_of("texelSize");
        const auto idUnder = material::Semantics::id_of("under");
        const auto idSelectedCount = material::Semantics::id_of("selectedCount");
        const auto idSelected = material::Semantics::id_of("selected");
        const auto under = with<system::Window>::get(world, window).current.under;
        const int selectedCount = std::min(static_cast<int>(selection.size()), resource::overlay::selection_capacity);
        for (const auto& binding : overlayRuntime.bindings) {
            if (binding.id == idScene)
                glBindTextureUnit(material::Semantics::binding_of(binding.id), sceneColor.color);
            else if (binding.id == idIdent)
                glBindTextureUnit(material::Semantics::binding_of(binding.id), identity.color);
            else if (binding.id == idSelectedMap)
                glBindTextureUnit(material::Semantics::binding_of(binding.id), identity.selected);
            else if (binding.location < 0)
                continue;
            else if (binding.id == idTexel)
                glUniform2f(binding.location, 1.0f / static_cast<float>(wh.x), 1.0f / static_cast<float>(wh.y));
            else if (binding.id == idUnder)
                glUniform1ui(binding.location, under);
            else if (binding.id == idSelectedCount)
                glUniform1i(binding.location, selectedCount);
            else if (binding.id == idSelected and selectedCount > 0)
                glUniform1uiv(binding.location, selectedCount, selection.data());
        }

        fullscreen.draw();
        glUseProgram(0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        system::Viewport::Actions::activate(world, viewport);
    }

    void OverlayCompose::compose(index2 size, gl::Triangle& fullscreen) {
        const auto wh = extent(size);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, wh.x, wh.y);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(composeProgram);
        glBindTextureUnit(0, overlayColor.color);
        fullscreen.draw();
        glUseProgram(0);
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
    }

}
