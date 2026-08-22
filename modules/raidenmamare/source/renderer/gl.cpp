#include "gl.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace rmmr::gl {

    namespace {

        auto compileStage(GLenum type, const char* src, const char* label) -> GLuint {
            GLuint shader = glCreateShader(type);
            glShaderSource(shader, 1, &src, nullptr);
            glCompileShader(shader);
            GLint ok = 0;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
            if (ok)
                return shader;
            GLint length = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
            std::string log(static_cast<std::size_t>(std::max(length, 1)), '\0');
            glGetShaderInfoLog(shader, length, nullptr, log.data());
            glDeleteShader(shader);
            throw std::runtime_error(std::string("Renderer: ") + label + " shader compile failed: " + log);
        }

        auto readText(const std::filesystem::path& path) -> std::string {
            std::ifstream input(path, std::ios::binary);
            if (not input)
                throw std::runtime_error("Renderer: cannot read shader " + path.string());
            return std::string{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
        }

    } // namespace

    auto extent(index2 size) -> index2 {
        return index2{std::max(static_cast<int>(size.x), 1), std::max(static_cast<int>(size.y), 1)};
    }

    void releaseTexture(renderer::Texture& texture) {
        if (not texture)
            return;
        glDeleteTextures(1, &texture);
        texture = 0;
    }

    void releaseFramebuffer(renderer::Framebuffer& fbo) {
        if (not fbo)
            return;
        glDeleteFramebuffers(1, &fbo);
        fbo = 0;
    }

    void releaseProgram(renderer::Program& program) {
        if (not program)
            return;
        glDeleteProgram(program);
        program = 0;
    }

    void releaseVertexArray(renderer::VertexArray& vao) {
        if (not vao)
            return;
        glDeleteVertexArrays(1, &vao);
        vao = 0;
    }

    auto makeTexture2D(index2 size, GLenum internalFormat, GLenum minFilter, GLenum magFilter) -> renderer::Texture {
        const auto wh = extent(size);
        renderer::Texture texture = 0;
        glCreateTextures(GL_TEXTURE_2D, 1, &texture);
        glTextureStorage2D(texture, 1, internalFormat, wh.x, wh.y);
        glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, minFilter);
        glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, magFilter);
        glTextureParameteri(texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        return texture;
    }

    auto makeFramebuffer() -> renderer::Framebuffer {
        renderer::Framebuffer fbo = 0;
        glCreateFramebuffers(1, &fbo);
        return fbo;
    }

    void attachColor(renderer::Framebuffer fbo, int slot, renderer::Texture texture) {
        glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0 + slot, texture, 0);
    }

    void attachDepth(renderer::Framebuffer fbo, renderer::Texture texture) {
        glNamedFramebufferTexture(fbo, GL_DEPTH_ATTACHMENT, texture, 0);
    }

    void finishFramebuffer(renderer::Framebuffer fbo, int colorSlots, const char* label) {
        std::vector<GLenum> buffers;
        buffers.reserve(static_cast<std::size_t>(colorSlots));
        for (int slot = 0; slot < colorSlots; ++slot)
            buffers.push_back(static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + slot));
        glNamedFramebufferDrawBuffers(fbo, colorSlots, buffers.data());
        if (glCheckNamedFramebufferStatus(fbo, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            throw std::runtime_error(std::string("Renderer: ") + label + " framebuffer incomplete");
    }

    void destroy(ColorTarget& target) {
        releaseFramebuffer(target.fbo);
        releaseTexture(target.color);
        target.size = index2{0, 0};
    }

    void ensureColorTarget(ColorTarget& target, index2 size, GLenum internalFormat, GLenum filter, const char* label) {
        if (target.fbo and target.size.x == size.x and target.size.y == size.y)
            return;
        destroy(target);
        target.color = makeTexture2D(size, internalFormat, filter, filter);
        target.fbo = makeFramebuffer();
        attachColor(target.fbo, 0, target.color);
        finishFramebuffer(target.fbo, 1, label);
        target.size = size;
    }

    auto compileProgram(const char* vertexSrc, const char* fragmentSrc, const char* label) -> renderer::Program {
        const GLuint vs = compileStage(GL_VERTEX_SHADER, vertexSrc, label);
        const GLuint fs = compileStage(GL_FRAGMENT_SHADER, fragmentSrc, label);
        renderer::Program program = glCreateProgram();
        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glLinkProgram(program);
        glDeleteShader(vs);
        glDeleteShader(fs);
        GLint linked = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &linked);
        if (linked)
            return program;
        GLint length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        std::string log(static_cast<std::size_t>(std::max(length, 1)), '\0');
        glGetProgramInfoLog(program, length, nullptr, log.data());
        glDeleteProgram(program);
        throw std::runtime_error(std::string("Renderer: ") + label + " program link failed: " + log);
    }

    auto compileProgramFromFiles(const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath, const char* label) -> renderer::Program {
        const auto vertexSrc = readText(vertexPath);
        const auto fragmentSrc = readText(fragmentPath);
        return compileProgram(vertexSrc.c_str(), fragmentSrc.c_str(), label);
    }

    void Triangle::destroy() {
        releaseVertexArray(vao);
    }

    void Triangle::draw() {
        if (vao == 0)
            glCreateVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);
    }

}
