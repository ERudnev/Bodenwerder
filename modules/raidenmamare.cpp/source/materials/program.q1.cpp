#include <Raidenmamare/materials/program.q1.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>

namespace rmmr::material {
    struct Program_private : Program::Operations {
        using Passport = Program::Materializer::Passport;

        static auto read_text_file(const std::filesystem::path& path) -> std::string {
            std::ifstream input(path, std::ios::binary);
            if (!input) {
                std::cerr << "Program: failed to open file: " << path << std::endl;
                return {};
            }
            return {
                std::istreambuf_iterator<char>(input),
                std::istreambuf_iterator<char>()
            };
        }

        static auto resolve_program_path(
            const std::filesystem::path& asset_root,
            std::string_view filename) -> std::filesystem::path
        {
            const std::filesystem::path path(filename);
            if (path.is_absolute()) return path;
            if (asset_root.empty()) {
                throw std::runtime_error("Program::Materializer::materialize: asset root is empty");
            }
            return asset_root / path;
        }

        static auto compile_shader(GLenum shader_type, const std::string& source, const char* label) -> GLuint {
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
            throw std::runtime_error(std::string("Program::compile_shader: ") + label + ": " + info_log);
        }

        static auto create_program(const std::filesystem::path& asset_root, const Passport& passport) -> GLuint {
            const auto vertex_path = resolve_program_path(asset_root, passport.vertexFilename);
            const auto fragment_path = resolve_program_path(asset_root, passport.fragmentFilename);

            const std::string vertex_source = read_text_file(vertex_path);
            if (vertex_source.empty()) {
                throw std::runtime_error(
                    "Program::create_program: vertex shader source is empty or unreadable: " + vertex_path.string());
            }

            const std::string fragment_source = read_text_file(fragment_path);
            if (fragment_source.empty()) {
                throw std::runtime_error(
                    "Program::create_program: fragment shader source is empty or unreadable: " + fragment_path.string());
            }

            const GLuint vertex_shader =
                compile_shader(GL_VERTEX_SHADER, vertex_source, passport.vertexFilename.c_str());
            const GLuint fragment_shader =
                compile_shader(GL_FRAGMENT_SHADER, fragment_source, passport.fragmentFilename.c_str());

            const GLuint program = glCreateProgram();
            if (!program) {
                glDeleteShader(vertex_shader);
                glDeleteShader(fragment_shader);
                throw std::runtime_error("Program::create_program: glCreateProgram() failed");
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
                throw std::runtime_error(std::string("Program::create_program: link failed: ") + info_log);
            }

            glDeleteShader(vertex_shader);
            glDeleteShader(fragment_shader);
            return program;
        }

        static void release_program(GLuint program) {
            if (!program) return;
            glDeleteProgram(program);
        }
    };

    void Program::Materializer::materialize(resources::Manager manager, Reading world, Program::Id id) const {
        if (manager->materialized<Program>(id)) {
            throw std::runtime_error("Program::Materializer::materialize: resource is already materialized");
        }

        const auto& quantum = ops::particle::get<Program>(world, id);
        const auto& passport = quantum.passport;
        const auto& devicePassport = ops::particle::get<rmmr::Device>(world, quantum.device).passport;
        if (!rmmr::Device::Operations::provide(world, quantum.device, manager)) {
            throw std::runtime_error("Program::Materializer::materialize: device is not open");
        }

        const std::filesystem::path asset_root(devicePassport.assets_root);
        const GLuint program = Program_private::create_program(asset_root, passport);
        manager->layer<Program>().materialize(id, program);
    }

    void Program::Materializer::release(resources::Manager manager, Reading, Id id) const {
        auto& layer = manager->layer<Program>();
        const GLuint program = layer.value(id);
        Program_private::release_program(program);
        layer.release(id);
    }

    void Program::Operations::materialize(Reading world, Id id, resources::Manager manager) {
        ops::resource::materialize<Program>(world, manager, id);
    }

    void Program::Operations::release(Reading world, Id id, resources::Manager manager) {
        ops::resource::release<Program>(world, manager, id);
    }

    auto Program::Operations::provide(Reading, Id id, resources::Manager manager) -> RuntimeAccess {
        return manager->layer<Program>().provide(id);
    }

    const Invariants Program::invariants{
        .structural = {},
        .logical = {},
    };
}
