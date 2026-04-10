#include <Raidenmamare/program.q1.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>

namespace rmmr {
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
            std::cerr << "Program: " << label << " compilation failed: " << info_log << std::endl;
            glDeleteShader(shader);
            return 0;
        }

        static auto link_program(GLuint program) -> bool {
            glLinkProgram(program);

            int success = 0;
            glGetProgramiv(program, GL_LINK_STATUS, &success);
            if (success) return true;

            char info_log[2048];
            glGetProgramInfoLog(program, sizeof(info_log), nullptr, info_log);
            std::cerr << "Program: linking failed: " << info_log << std::endl;
            return false;
        }

        static auto create_program(const std::filesystem::path& asset_root, const Passport& passport) -> GLuint {
            const auto vertex_path = resolve_program_path(asset_root, passport.vertexFilename);
            const auto fragment_path = resolve_program_path(asset_root, passport.fragmentFilename);

            const std::string vertex_source = read_text_file(vertex_path);
            if (vertex_source.empty()) return 0;

            const std::string fragment_source = read_text_file(fragment_path);
            if (fragment_source.empty()) return 0;

            const GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_source, "vertex shader");
            if (!vertex_shader) return 0;

            const GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_source, "fragment shader");
            if (!fragment_shader) {
                glDeleteShader(vertex_shader);
                return 0;
            }

            const GLuint program = glCreateProgram();
            glAttachShader(program, vertex_shader);
            glAttachShader(program, fragment_shader);

            const bool linked = link_program(program);
            glDeleteShader(vertex_shader);
            glDeleteShader(fragment_shader);

            if (!linked) {
                glDeleteProgram(program);
                return 0;
            }

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
            const auto& corePassport = ops::particle::get<Core>(world, quantum.core).passport;
        if (!Core::Operations::provide(world, quantum.core, manager)) {
            throw std::runtime_error("Program::Materializer::materialize: core is not open");
        }

        const std::filesystem::path asset_root(corePassport.assets_root);
        const GLuint program = Program_private::create_program(asset_root, passport);
        if (!program) {
            throw std::runtime_error("Program::Materializer::materialize: failed to create OpenGL program");
        }
        manager->layer<Program>().materialize(id, program);
    }

    void Program::Materializer::release(resources::Manager manager, Reading, Id id) const {
        auto& layer = manager->layer<Program>();
        const GLuint program = layer.value(id);
        Program_private::release_program(program);
        layer.release(id);
    }

    void Program::Operations::open(Reading world, Id id, resources::Manager manager) {
        ops::resource::materialize<Program>(world, manager, id);
    }

    void Program::Operations::close(Reading world, Id id, resources::Manager manager) {
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
