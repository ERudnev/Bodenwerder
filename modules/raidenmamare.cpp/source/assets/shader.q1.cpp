#include <rmmr/assets/shader.q1.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string_view>

namespace rmmr::asset {

    using namespace fqsm::api;

    namespace {

        auto read_text_file(const std::filesystem::path& path) -> std::string {
            std::ifstream input(path, std::ios::binary);
            if (not input) {
                throw std::runtime_error("asset::Shader::compile: failed to open file: " + path.string());
            }
            return {
                std::istreambuf_iterator<char>(input),
                std::istreambuf_iterator<char>(),
            };
        }

        auto resolve_under_asset_root(const std::filesystem::path& asset_root, std::string_view relative_path) -> std::filesystem::path {
            const std::filesystem::path path(relative_path);
            if (path.is_absolute()) return path;
            if (asset_root.empty()) {
                throw std::runtime_error("asset::Shader::compile: asset root is empty");
            }
            return asset_root / path;
        }

        auto compile_shader(GLenum shader_type, const std::string& source, const char* label) -> GLuint {
            const GLuint shader = glCreateShader(shader_type);

            const char* source_ptr = source.c_str();
            glShaderSource(shader, 1, &source_ptr, nullptr);
            glCompileShader(shader);

            int success = 0;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (success) return shader;

            char info_log[2048];
            glGetShaderInfoLog(shader, sizeof(info_log), nullptr, info_log);
            glDeleteShader(shader);
            throw std::runtime_error(std::string("asset::Shader::compile: ") + label + ": " + info_log);
        }

        auto create_program(const std::filesystem::path& asset_root, const string& name, const string& library) -> GLuint {
            const std::string vertex_rel = Shader::Always::vertexFilename(name, library);
            const std::string fragment_rel = Shader::Always::fragmentFilename(name, library);

            const auto vertex_path = resolve_under_asset_root(asset_root, vertex_rel);
            const auto fragment_path = resolve_under_asset_root(asset_root, fragment_rel);

            const std::string vertex_source = read_text_file(vertex_path);
            if (vertex_source.empty()) {
                throw std::runtime_error("asset::Shader::compile: vertex shader source is empty or unreadable: " + vertex_path.string());
            }

            const std::string fragment_source = read_text_file(fragment_path);
            if (fragment_source.empty()) {
                throw std::runtime_error("asset::Shader::compile: fragment shader source is empty or unreadable: " + fragment_path.string());
            }

            const GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_source, vertex_rel.c_str());
            const GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_source, fragment_rel.c_str());

            const GLuint program = glCreateProgram();
            if (not program) {
                glDeleteShader(vertex_shader);
                glDeleteShader(fragment_shader);
                throw std::runtime_error("asset::Shader::compile: glCreateProgram() failed");
            }
            glAttachShader(program, vertex_shader);
            glAttachShader(program, fragment_shader);
            glLinkProgram(program);

            int link_ok = 0;
            glGetProgramiv(program, GL_LINK_STATUS, &link_ok);
            if (not link_ok) {
                char info_log[2048];
                glGetProgramInfoLog(program, sizeof(info_log), nullptr, info_log);
                glDeleteShader(vertex_shader);
                glDeleteShader(fragment_shader);
                glDeleteProgram(program);
                throw std::runtime_error(std::string("asset::Shader::compile: link failed: ") + info_log);
            }

            glDeleteShader(vertex_shader);
            glDeleteShader(fragment_shader);
            return program;
        }

    } // namespace

    auto Shader::Always::vertexFilename(const string& name, const string& library) -> string {
        if (library.empty()) {
            return "shaders/" + name + ".vert.glsl";
        }
        return library + "/shaders/" + name + ".vert.glsl";
    }

    auto Shader::Always::fragmentFilename(const string& name, const string& library) -> string {
        if (library.empty()) {
            return "shaders/" + name + ".frag.glsl";
        }
        return library + "/shaders/" + name + ".frag.glsl";
    }

    auto Shader::Actions::compile(Writing context, Id asset_shader, system::Device::Id device) -> resource::Shader::Id {
        const auto& asset = with<Shader>::get(context, asset_shader);
        const auto& device_quantum = with<system::Device>::get(context, device);
        const auto& core_quantum = with<system::Core>::get(context, device_quantum.core);

        glfwMakeContextCurrent(device_quantum.handle);

        const std::filesystem::path asset_root(core_quantum.assets_root);
        const GLuint program = create_program(asset_root, asset.name, asset.library);

        with<resource::Shader_group>::extend(context, device);
        return with<resource::Shader_group>::addElement(context, device, resource::Shader::Quantum{
            .handle = program,
        });
    }

}
