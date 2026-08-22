#pragma once

#include <rmmr/math.q1.h>
#include <rmmr/renderer/gl.q1.h>

#include <GL/glew.h>

#include <filesystem>

namespace rmmr::gl {

    using namespace fqsm::api;

    auto extent(index2 size) -> index2;

    void releaseTexture(renderer::Texture& texture);
    void releaseFramebuffer(renderer::Framebuffer& fbo);
    void releaseProgram(renderer::Program& program);
    void releaseVertexArray(renderer::VertexArray& vao);

    auto makeTexture2D(index2 size, GLenum internalFormat, GLenum minFilter, GLenum magFilter) -> renderer::Texture;
    auto makeFramebuffer() -> renderer::Framebuffer;
    void attachColor(renderer::Framebuffer fbo, int slot, renderer::Texture texture);
    void attachDepth(renderer::Framebuffer fbo, renderer::Texture texture);
    void finishFramebuffer(renderer::Framebuffer fbo, int colorSlots, const char* label);

    struct ColorTarget {
        renderer::Framebuffer fbo;
        renderer::Texture color;
        index2 size;
    };

    void destroy(ColorTarget& target);
    void ensureColorTarget(ColorTarget& target, index2 size, GLenum internalFormat, GLenum filter, const char* label);

    auto compileProgram(const char* vertexSrc, const char* fragmentSrc, const char* label) -> renderer::Program;
    auto compileProgramFromFiles(const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath, const char* label) -> renderer::Program;

    struct Triangle {
        renderer::VertexArray vao;
        void destroy();
        void draw();
    };

}
