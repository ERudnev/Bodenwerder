#include "bloom.h"

#include <algorithm>

namespace rmmr {

    using namespace gl;

    namespace {
        constexpr int bloomScale = 4;

        auto bloomSizeFor(index2 sceneSize) -> index2 {
            return index2{std::max(static_cast<int>(sceneSize.x) / bloomScale, 1), std::max(static_cast<int>(sceneSize.y) / bloomScale, 1)};
        }
    }

    void Bloom::destroy() {
        releaseFramebuffer(sourceFbo);
        releaseFramebuffer(scratchFbo);
        releaseTexture(source);
        releaseTexture(scratch);
        releaseProgram(downsampleProgram);
        releaseProgram(blurProgram);
        releaseProgram(tonemapProgram);
        size = index2{0, 0};
    }

    void Bloom::ensure(index2 sceneSize) {
        const index2 bloomSize = bloomSizeFor(sceneSize);
        if (sourceFbo and size.x == bloomSize.x and size.y == bloomSize.y)
            return;
        releaseFramebuffer(sourceFbo);
        releaseFramebuffer(scratchFbo);
        releaseTexture(source);
        releaseTexture(scratch);
        source = makeTexture2D(bloomSize, GL_RGBA16F, GL_LINEAR, GL_LINEAR);
        scratch = makeTexture2D(bloomSize, GL_RGBA16F, GL_LINEAR, GL_LINEAR);
        sourceFbo = makeFramebuffer();
        attachColor(sourceFbo, 0, source);
        finishFramebuffer(sourceFbo, 1, "bloom source");
        scratchFbo = makeFramebuffer();
        attachColor(scratchFbo, 0, scratch);
        finishFramebuffer(scratchFbo, 1, "bloom scratch");
        size = bloomSize;
    }

    void Bloom::ensurePrograms(const std::filesystem::path& shaders) {
        if (not downsampleProgram)
            downsampleProgram = compileProgramFromFiles(shaders / "fullscreen.vert.glsl", shaders / "bloomDownsample.frag.glsl", "bloom downsample");
        if (not blurProgram)
            blurProgram = compileProgramFromFiles(shaders / "fullscreen.vert.glsl", shaders / "bloomBlur.frag.glsl", "bloom blur");
        if (not tonemapProgram)
            tonemapProgram = compileProgramFromFiles(shaders / "fullscreen.vert.glsl", shaders / "tonemap.frag.glsl", "tonemap");
    }

    void Bloom::downsample(renderer::Texture hdr, renderer::Texture bloomMask, gl::Triangle& fullscreen) {
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        const auto wh = extent(size);
        glBindFramebuffer(GL_FRAMEBUFFER, sourceFbo);
        glViewport(0, 0, wh.x, wh.y);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glDisable(GL_BLEND);
        glUseProgram(downsampleProgram);
        glBindTextureUnit(0, hdr);
        glBindTextureUnit(1, bloomMask);
        fullscreen.draw();
        glUseProgram(0);
    }

    void Bloom::blur(float radius, gl::Triangle& fullscreen) {
        const auto wh = extent(size);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glDisable(GL_BLEND);
        glUseProgram(blurProgram);
        glUniform1f(glGetUniformLocation(blurProgram, "u_radius"), std::max(radius, 0.0f));
        const GLint texelDirLoc = glGetUniformLocation(blurProgram, "u_texelDir");

        glBindFramebuffer(GL_FRAMEBUFFER, scratchFbo);
        glViewport(0, 0, wh.x, wh.y);
        glBindTextureUnit(0, source);
        glUniform2f(texelDirLoc, 1.0f / static_cast<float>(wh.x), 0.0f);
        fullscreen.draw();

        glBindFramebuffer(GL_FRAMEBUFFER, sourceFbo);
        glBindTextureUnit(0, scratch);
        glUniform2f(texelDirLoc, 0.0f, 1.0f / static_cast<float>(wh.y));
        fullscreen.draw();
        glUseProgram(0);
    }

    void Bloom::tonemapToWindow(Writing world, system::Viewport::Id viewport, renderer::Texture hdr, float intensity, gl::Triangle& fullscreen) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        system::Viewport::Actions::activate(world, viewport);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glDisable(GL_BLEND);
        glUseProgram(tonemapProgram);
        glBindTextureUnit(0, hdr);
        glBindTextureUnit(1, source);
        glUniform1f(glGetUniformLocation(tonemapProgram, "u_intensity"), std::max(intensity, 0.0f));
        fullscreen.draw();
        glUseProgram(0);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
    }

}
