#include <rmmr/resources/shaders.q1.h>
#include <rmmr/resources/runtimes.q1.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <base/logging.h>
#include <base/maybe.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace rmmr::resource::shader {

    using namespace fqsm::api;

    namespace {

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

        auto shaderInfoLog(GLuint shader) -> std::string {
            GLint length = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
            if (length <= 1)
                return {};
            std::string log(static_cast<std::size_t>(length), '\0');
            glGetShaderInfoLog(shader, length, nullptr, log.data());
            while (not log.empty() and log.back() == '\0')
                log.pop_back();
            return log;
        }

        auto programInfoLog(GLuint program) -> std::string {
            GLint length = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
            if (length <= 1)
                return {};
            std::string log(static_cast<std::size_t>(length), '\0');
            glGetProgramInfoLog(program, length, nullptr, log.data());
            while (not log.empty() and log.back() == '\0')
                log.pop_back();
            return log;
        }

        auto compile_shader_stage(GLenum shader_type, const std::string& source, std::string& log) -> maybe<GLuint> {
            const GLuint shader = glCreateShader(shader_type);
            const char* source_ptr = source.c_str();
            glShaderSource(shader, 1, &source_ptr, nullptr);
            glCompileShader(shader);
            int success = 0;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (success)
                return shader;
            log = shaderInfoLog(shader);
            glDeleteShader(shader);
            return {};
        }

        void release_gl(Writing context, const Runtime::Quantum& last) {
            if (not last.handle) {
                return;
            }
            glfwMakeContextCurrent(with<system::Device>::get(context, last.device).handle);
            glDeleteProgram(last.handle);
        }

        auto install_runtime(Writing context, system::Device::Id device, Asset::Id asset_id, Runtime::Quantum quantum) -> Runtime::Id {
            const auto& runtimes = with<Runtimes>::get(context, device);
            if (const auto existing = runtimes.shaders_id_mapping.find(asset_id); existing != runtimes.shaders_id_mapping.end()) {
                if (with<Runtime>::exists(context, existing->second)) {
                    auto runtime = with<Runtime>::modify(context, existing->second);
                    release_gl(context, *runtime);
                    *runtime = std::move(quantum);
                    return existing->second;
                }
            }
            return with<ShaderRuntime_group>::addElement(context, device, std::move(quantum));
        }

    } // namespace

    auto Loader::Actions::materialize(Writing context, Id asset_id, system::Device::Id device) -> optional<Runtime::Id> {
        const auto& loader = with<Loader>::get(context, asset_id);
        const auto& unit = with<Unit>::get(context, asset_id);

        const auto& device_quantum = with<system::Device>::get(context, device);
        glfwMakeContextCurrent(device_quantum.handle);

        const auto vertex_path = with<Manager>::resolve(context, unit, loader.vertex);
        const auto fragment_path = with<Manager>::resolve(context, unit, loader.fragment);
        base::whisper("rmmr: shader::Loader '{}' ← {} + {}", unit.name.text(), vertex_path.string(), fragment_path.string());

        const auto vertex_source = read_text_file(vertex_path);
        if (not vertex_source or vertex_source->empty())
            return context.refuse("resource::shader::Loader::materialize: vertex shader unreadable: " + vertex_path.string());

        const auto fragment_source = read_text_file(fragment_path);
        if (not fragment_source or fragment_source->empty())
            return context.refuse("resource::shader::Loader::materialize: fragment shader unreadable: " + fragment_path.string());

        std::string vertexLog;
        const auto vertex_shader = compile_shader_stage(GL_VERTEX_SHADER, *vertex_source, vertexLog);
        if (not vertex_shader)
            return context.refuse("resource::shader::Loader::materialize: vertex shader compile failed: " + std::string(loader.vertex) + "\n" + vertexLog);

        std::string fragmentLog;
        const auto fragment_shader = compile_shader_stage(GL_FRAGMENT_SHADER, *fragment_source, fragmentLog);
        if (not fragment_shader) {
            glDeleteShader(*vertex_shader);
            return context.refuse("resource::shader::Loader::materialize: fragment shader compile failed: " + std::string(loader.fragment) + "\n" + fragmentLog);
        }

        const GLuint program = glCreateProgram();
        if (not program) {
            glDeleteShader(*vertex_shader);
            glDeleteShader(*fragment_shader);
            return context.refuse("resource::shader::Loader::materialize: glCreateProgram failed");
        }

        glAttachShader(program, *vertex_shader);
        glAttachShader(program, *fragment_shader);
        glLinkProgram(program);

        int link_ok = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &link_ok);
        glDeleteShader(*vertex_shader);
        glDeleteShader(*fragment_shader);

        if (not link_ok) {
            const std::string log = programInfoLog(program);
            glDeleteProgram(program);
            return context.refuse("resource::shader::Loader::materialize: program link failed:\n" + log);
        }

        return install_runtime(context, device, asset_id, Runtime::Quantum{
            .device = device,
            .handle = program,
        });
    }

    struct Runtime::Internals : Runtime::DefaultInternals {
        static void release(Writing context, Id, const Quantum& last) {
            release_gl(context, last);
        }
    };

    auto Runtime::customAspectReactions() -> const Behavior {
        return {
            reaction::deletion<Runtime>(&Runtime::Internals::release),
        };
    }

}
