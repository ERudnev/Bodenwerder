#include "nearShadow.h"

#include <stdexcept>

namespace rmmr {

    static_assert(sizeof(NearShadowFrame) == 80);

    void NearShadow::prepare(const NearShadowFrame& next) {
        frame = next;
        if (frame.cameraRange.w > 0.0f and not depth) {
            GLint textureUnits = 0;
            glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &textureUnits);
            if (format.textureUnit >= static_cast<unsigned>(textureUnits))
                throw std::runtime_error("NearShadow: insufficient fragment texture units");
            glCreateTextures(GL_TEXTURE_2D, 1, &depth);
            glTextureStorage2D(depth, 1, GL_DEPTH_COMPONENT24, format.resolution, format.resolution);
            glTextureParameteri(depth, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTextureParameteri(depth, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTextureParameteri(depth, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
            glTextureParameteri(depth, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
            const float border[]{1.0f, 1.0f, 1.0f, 1.0f};
            glTextureParameterfv(depth, GL_TEXTURE_BORDER_COLOR, border);
            glCreateFramebuffers(1, &fbo);
            glNamedFramebufferTexture(fbo, GL_DEPTH_ATTACHMENT, depth, 0);
            glNamedFramebufferDrawBuffer(fbo, GL_NONE);
            glNamedFramebufferReadBuffer(fbo, GL_NONE);
            if (glCheckNamedFramebufferStatus(fbo, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                throw std::runtime_error("NearShadow: incomplete depth framebuffer");
        }
        if (not stateBuffer) {
            glCreateBuffers(1, &stateBuffer);
            glNamedBufferData(stateBuffer, sizeof(frame), &frame, GL_DYNAMIC_DRAW);
        } else {
            glNamedBufferSubData(stateBuffer, 0, sizeof(frame), &frame);
        }
        bind();
    }

    void NearShadow::begin() {
        glBindTextureUnit(format.textureUnit, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, format.resolution, format.resolution);
        glDepthMask(GL_TRUE);
        glClear(GL_DEPTH_BUFFER_BIT);
    }

    void NearShadow::bind() {
        // Frame-owned bindings; material texture slots 3..18 remain untouched.
        glBindBufferBase(GL_UNIFORM_BUFFER, format.stateBinding, stateBuffer);
        glBindTextureUnit(format.textureUnit, depth);
    }

    void NearShadow::destroy() {
        if (stateBuffer) glDeleteBuffers(1, &stateBuffer);
        stateBuffer = 0;
        gl::releaseFramebuffer(fbo);
        gl::releaseTexture(depth);
    }

}
