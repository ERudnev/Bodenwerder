#include <Raidenmamare/materials/program.q1.h>

#include <GLFW/glfw3.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>

namespace rmmr::material {
    namespace {
        auto read_text_file(const std::filesystem::path& path) -> std::string {
            std::ifstream input(path, std::ios::binary);
            if (!input) {
                throw std::runtime_error("Program::Actions::compile: unreadable file: " + path.string());
            }
            const std::string contents{
                std::istreambuf_iterator<char>(input),
                std::istreambuf_iterator<char>(),
            };
            if (contents.empty()) {
                throw std::runtime_error("Program::Actions::compile: empty file: " + path.string());
            }
            return contents;
        }

        auto resolve_under_asset_root(const string& asset_root, std::string_view relative_path) -> std::filesystem::path {
            const std::filesystem::path path(relative_path);
            if (path.is_absolute()) return path;
            return std::filesystem::path(asset_root) / path;
        }

        auto compile_shader(GLenum shader_type, const std::string& source, const char* label) -> GLuint {
            const GLuint shader = glCreateShader(shader_type);

            const char* source_ptr = source.c_str();
            glShaderSource(shader, 1, &source_ptr, nullptr);
            glCompileShader(shader);

            int success = 0;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (success) {
                return shader;
            }

            char info_log[2048];
            glGetShaderInfoLog(shader, sizeof(info_log), nullptr, info_log);
            glDeleteShader(shader);
            throw std::runtime_error(std::string("Program::Actions::compile: ") + label + ": " + info_log);
        }

        auto link_program(const std::string& vertex_source, const std::string& fragment_source, const char* vertex_label, const char* fragment_label) -> GLuint {
            const GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_source, vertex_label);
            const GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_source, fragment_label);

            const GLuint program = glCreateProgram();
            if (!program) {
                glDeleteShader(vertex_shader);
                glDeleteShader(fragment_shader);
                throw std::runtime_error("Program::Actions::compile: glCreateProgram() failed");
            }

            glAttachShader(program, vertex_shader);
            glAttachShader(program, fragment_shader);
            glLinkProgram(program);

            int link_ok = 0;
            glGetProgramiv(program, GL_LINK_STATUS, &link_ok);
            if (!link_ok) {
                char info_log[2048];
                glGetProgramInfoLog(program, sizeof(info_log), nullptr, info_log);
                glDeleteShader(vertex_shader);
                glDeleteShader(fragment_shader);
                glDeleteProgram(program);
                throw std::runtime_error(std::string("Program::Actions::compile: link failed: ") + info_log);
            }

            glDeleteShader(vertex_shader);
            glDeleteShader(fragment_shader);
            return program;
        }

        auto create_program(const string& asset_root, const Program::Quantum& quantum) -> GLuint {
            const std::string vertex_rel = Program::Actions::vertexFilename(quantum.name, quantum.library);
            const std::string fragment_rel = Program::Actions::fragmentFilename(quantum.name, quantum.library);

            const auto vertex_path = resolve_under_asset_root(asset_root, vertex_rel);
            const auto fragment_path = resolve_under_asset_root(asset_root, fragment_rel);

            const std::string vertex_source = read_text_file(vertex_path);
            const std::string fragment_source = read_text_file(fragment_path);

            return link_program(vertex_source, fragment_source, vertex_rel.c_str(), fragment_rel.c_str());
        }
    }

    struct Program::Internals : Program::DefaultInternals {
        static void release(Writing, Id, const Quantum& last) {
            if (!last.handle) {
                return;
            }
            glDeleteProgram(last.handle);
        }
    };

    auto Program::Actions::vertexFilename(string name, string library) -> string {
        if (library.empty()) {
            return "shaders/" + name + ".vert.glsl";
        }
        return library + "/shaders/" + name + ".vert.glsl";
    }

    auto Program::Actions::fragmentFilename(string name, string library) -> string {
        if (library.empty()) {
            return "shaders/" + name + ".frag.glsl";
        }
        return library + "/shaders/" + name + ".frag.glsl";
    }

    void Program::Actions::compile(Writing context, Id id, Window::Id window) {
        const auto& quantum = get(context, id);
        if (quantum.handle)
            throw std::runtime_error("Program::Actions::compile: program is already compiled");

        glfwMakeContextCurrent(with<Window>::get(context, window).handle);
        modify(context, id)->handle = create_program(with<Application>::get_global(context).assets_root, quantum);
    }

    auto Program::customAspectReactions() -> const Behavior {
        return {
            reaction::deletion<Program>(&Internals::release),
        };
    }
}
