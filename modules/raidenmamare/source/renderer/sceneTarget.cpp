#include "sceneTarget.h"

namespace rmmr {

    using namespace gl;

    void SceneTarget::destroy() {
        releaseFramebuffer(fbo);
        releaseTexture(hdr);
        releaseTexture(bloomMask);
        releaseTexture(depth);
        size = index2{0, 0};
    }

    void SceneTarget::ensure(index2 targetSize) {
        if (fbo and size.x == targetSize.x and size.y == targetSize.y)
            return;
        destroy();
        hdr = makeTexture2D(targetSize, GL_RGBA16F, GL_LINEAR, GL_LINEAR);
        bloomMask = makeTexture2D(targetSize, GL_R16F, GL_NEAREST, GL_NEAREST);
        depth = makeTexture2D(targetSize, GL_DEPTH_COMPONENT24, GL_NEAREST, GL_NEAREST);
        fbo = makeFramebuffer();
        attachColor(fbo, 0, hdr);
        attachColor(fbo, 1, bloomMask);
        attachDepth(fbo, depth);
        finishFramebuffer(fbo, 2, "scene HDR");
        size = targetSize;
    }

    void SceneTarget::setGlowWrite(bool on) {
        const GLboolean mask = on ? GL_TRUE : GL_FALSE;
        glColorMaski(1, mask, mask, mask, mask);
    }

    void SceneTarget::bind(index2 size) {
        ensure(size);
        const auto wh = extent(size);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, wh.x, wh.y);
        glDisablei(GL_BLEND, 1);
        setGlowWrite(false);
    }

    void SceneTarget::begin(index2 size, vec4 clearColor) {
        bind(size);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        const float hdrClear[]{clearColor.x, clearColor.y, clearColor.z, clearColor.w};
        const float maskClear[]{0.0f, 0.0f, 0.0f, 0.0f};
        glClearBufferfv(GL_COLOR, 0, hdrClear);
        glClearBufferfv(GL_COLOR, 1, maskClear);
        glClear(GL_DEPTH_BUFFER_BIT);
        setGlowWrite(false);
    }

}
