#include <rmmr/resources/shaders.q1.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <base/maybe.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace rmmr::resource::shader {

    using namespace fqsm::api;

    namespace {

        auto resolve_under_manager(const Manager::Quantum& manager, const Unit::Quantum& unit, const filename& relative) -> filepath {
            const std::filesystem::path file_path(relative);
            if (file_path.is_absolute()) {
                return file_path;
            }
            if (unit.library.empty()) {
                return manager.location / file_path;
            }
            return manager.location / unit.library / file_path;
        }

        auto read_text_file(const std::filesystem::path& path) -> maybe<std::string> {
            std::ifstream input(path, std::ios::binary);
            if (not input) {
                return {};
            }
            return std::string{
                std::istreambuf_iterator<char>(input),
                std::istreambuf_iterator<char>(),
            };
        }

        auto compile_shader_stage(GLenum shader_type, const std::string& source) -> maybe<GLuint> {
            const GLuint shader = glCreateShader(shader_type);
            const char* source_ptr = source.c_str();
            glShaderSource(shader, 1, &source_ptr, nullptr);
            glCompileShader(shader);

            int success = 0;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (success) {
                return shader;
            }

            glDeleteShader(shader);
            return {};
        }

    } // namespace

    auto FromFile::Actions::materialize(Writing context, Id asset_id, system::Device::Id device) -> Runtime::Quantum {
        const auto& from_file = with<FromFile>::get(context, asset_id);
        const auto& unit = with<Unit>::get(context, asset_id);
        const auto& manager = with<Manager>::get(context, unit.manager);

        const auto& device_quantum = with<system::Device>::get(context, device);
        glfwMakeContextCurrent(device_quantum.handle);

        const auto vertex_path = resolve_under_manager(manager, unit, from_file.vertex);
        const auto fragment_path = resolve_under_manager(manager, unit, from_file.fragment);

        const auto vertex_source = read_text_file(vertex_path);
        if (not vertex_source or vertex_source->empty())
            return context.refuse("resource::shader::FromFile::materialize: vertex shader unreadable: " + vertex_path.string());

        const auto fragment_source = read_text_file(fragment_path);
        if (not fragment_source or fragment_source->empty())
            return context.refuse("resource::shader::FromFile::materialize: fragment shader unreadable: " + fragment_path.string());

        const auto vertex_shader = compile_shader_stage(GL_VERTEX_SHADER, *vertex_source);
        if (not vertex_shader)
            return context.refuse("resource::shader::FromFile::materialize: vertex shader compile failed: " + std::string(from_file.vertex));

        const auto fragment_shader = compile_shader_stage(GL_FRAGMENT_SHADER, *fragment_source);
        if (not fragment_shader) {
            glDeleteShader(*vertex_shader);
            return context.refuse("resource::shader::FromFile::materialize: fragment shader compile failed: " + std::string(from_file.fragment));
        }

        const GLuint program = glCreateProgram();
        if (not program) {
            glDeleteShader(*vertex_shader);
            glDeleteShader(*fragment_shader);
            return context.refuse("resource::shader::FromFile::materialize: glCreateProgram failed");
        }

        glAttachShader(program, *vertex_shader);
        glAttachShader(program, *fragment_shader);
        glLinkProgram(program);

        int link_ok = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &link_ok);
        glDeleteShader(*vertex_shader);
        glDeleteShader(*fragment_shader);

        if (not link_ok) {
            char info_log[2048];
            glGetProgramInfoLog(program, sizeof(info_log), nullptr, info_log);
            glDeleteProgram(program);
            return context.refuse(std::string("resource::shader::FromFile::materialize: program link failed: ") + info_log);
        }

        return Runtime::Quantum{
            .device = device,
            .handle = program,
        };
    }

    struct Runtime::Internals : Runtime::DefaultInternals {
        static void release(Writing context, Id, const Quantum& last) {
            if (not last.handle) {
                return;
            }

            glfwMakeContextCurrent(with<system::Device>::get(context, last.device).handle);
            glDeleteProgram(last.handle);
        }
    };

    auto Runtime::customAspectReactions() -> const Behavior {
        return {
            reaction::deletion<Runtime>(&Runtime::Internals::release),
        };
    }

}
