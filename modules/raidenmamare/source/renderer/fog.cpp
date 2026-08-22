#include "fog.h"

#include <algorithm>
#include <glm/gtc/type_ptr.hpp>

namespace rmmr {

    using namespace gl;

    void FogPass::destroy() {
        gl::destroy(target);
        releaseProgram(program);
    }

    void FogPass::ensurePrograms(const std::filesystem::path& shaders) {
        if (not program)
            program = compileProgramFromFiles(shaders / "fullscreen.vert.glsl", shaders / "heightFog.frag.glsl", "height fog");
    }

    auto FogPass::apply(index2 size, const scene::Root::Quantum::Fog& fog, const mat4& invViewProj, vec3 cameraPos, renderer::Texture hdr, renderer::Texture depth, gl::Triangle& fullscreen) -> renderer::Texture {
        ensureColorTarget(target, size, GL_RGBA16F, GL_LINEAR, "height fog");
        const auto wh = extent(size);
        glBindFramebuffer(GL_FRAMEBUFFER, target.fbo);
        glViewport(0, 0, wh.x, wh.y);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glDisable(GL_BLEND);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glUseProgram(program);
        glBindTextureUnit(0, hdr);
        glBindTextureUnit(1, depth);
        glUniformMatrix4fv(glGetUniformLocation(program, "u_invViewProj"), 1, GL_FALSE, glm::value_ptr(invViewProj));
        glUniform3f(glGetUniformLocation(program, "u_cameraPos"), cameraPos.x, cameraPos.y, cameraPos.z);
        glUniform3f(glGetUniformLocation(program, "u_fogColor"), fog.color.x, fog.color.y, fog.color.z);
        glUniform1f(glGetUniformLocation(program, "u_fogDensity"), std::max(fog.density, 0.0f));
        glUniform1f(glGetUniformLocation(program, "u_fogHeight"), fog.height);
        glUniform1f(glGetUniformLocation(program, "u_fogHeightFalloff"), std::max(fog.heightFalloff, 0.0f));
        glUniform1f(glGetUniformLocation(program, "u_fogMaxOpacity"), std::clamp(fog.maxOpacity, 0.0f, 1.0f));
        glUniform1f(glGetUniformLocation(program, "u_fogDistanceScale"), std::max(fog.distanceScale, 0.0f));
        fullscreen.draw();
        glUseProgram(0);
        return target.color;
    }

}
