#include <Raidenmamare/program.q1.h>

#include <GL/glew.h>

#include <base/logging.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>

namespace rmmr {

    namespace {
        struct OpenglProgram final : iqsm::binding::resource::Data {
            GLuint handle = 0;

            explicit OpenglProgram(GLuint handle)
                : handle(handle) {}

            ~OpenglProgram() override {
                if (!handle) return;
                _INCOMPLETE_;
            }
        };

        auto readTextFile(const std::filesystem::path& path) -> std::string {
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

        auto resolveProgramPath(
            const std::filesystem::path& assetRoot,
            std::string_view filename) -> std::filesystem::path
        {
            const std::filesystem::path p(filename);
            if (p.is_absolute()) return p;
            if (assetRoot.empty()) _INCOMPLETE_;
            return assetRoot / p;
        }

        auto compileShader(GLenum shaderType, const std::string& source, const char* label) -> GLuint {
            const GLuint shader = glCreateShader(shaderType);

            const char* sourcePtr = source.c_str();
            glShaderSource(shader, 1, &sourcePtr, nullptr);
            glCompileShader(shader);

            int success = 0;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (success) return shader;

            char infoLog[2048];
            glGetShaderInfoLog(shader, sizeof(infoLog), nullptr, infoLog);
            std::cerr << "Program: " << label << " compilation failed: " << infoLog << std::endl;
            glDeleteShader(shader);
            return 0;
        }

        auto linkProgram(GLuint program) -> bool {
            glLinkProgram(program);

            int success = 0;
            glGetProgramiv(program, GL_LINK_STATUS, &success);
            if (success) return true;

            char infoLog[2048];
            glGetProgramInfoLog(program, sizeof(infoLog), nullptr, infoLog);
            std::cerr << "Program: linking failed: " << infoLog << std::endl;
            return false;
        }

        auto buildProgramFromFiles(
            const std::filesystem::path& assetRoot,
            const Program::Passport& passport) -> GLuint
        {
            const auto vertexPath = resolveProgramPath(assetRoot, passport.vertexFilename);
            const auto fragmentPath = resolveProgramPath(assetRoot, passport.fragmentFilename);

            const std::string vertexSource = readTextFile(vertexPath);
            if (vertexSource.empty()) return 0;

            const std::string fragmentSource = readTextFile(fragmentPath);
            if (fragmentSource.empty()) return 0;

            const GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource, "vertex shader");
            if (!vertexShader) return 0;

            const GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource, "fragment shader");
            if (!fragmentShader) {
                glDeleteShader(vertexShader);
                return 0;
            }

            const GLuint program = glCreateProgram();
            glAttachShader(program, vertexShader);
            glAttachShader(program, fragmentShader);

            const bool linked = linkProgram(program);
            glDeleteShader(vertexShader);
            glDeleteShader(fragmentShader);

            if (linked) return program;
            glDeleteProgram(program);
            return 0;
        }
    }

    struct OpenglProgramLoader final : iqsm::binding::resource::Loader<Program> {
        std::filesystem::path assetRoot;

        explicit OpenglProgramLoader(std::filesystem::path assetRoot)
            : assetRoot(std::move(assetRoot)) {}

        iqsm::binding::resource::Ptr load(iqsm::World world, iqsm::ref<iqsm::binding::ManagerData>, Program::Id id) const override {
            const auto& passport = ops::particle::get<Program>(world, id).passport;
            const GLuint program = buildProgramFromFiles(assetRoot, passport);
            if (!program) return {};
            return std::make_unique<OpenglProgram>(program);
        }

        void unload(iqsm::World, iqsm::binding::resource::Manager, Program::Id, iqsm::binding::resource::Data& data) const override {
            auto* program = dynamic_cast<OpenglProgram*>(&data);
            if (!program) _INCOMPLETE_;
            if (!program->handle) return;

            glDeleteProgram(program->handle);
            program->handle = 0;
        }
    };

    auto tryGetOpenglHandle(const iqsm::binding::resource::Data& data) -> GLuint {
        const auto* program = dynamic_cast<const OpenglProgram*>(&data);
        if (!program) return 0;
        return program->handle;
    }

    const Invariants Program::invariants{
        .structural = {},
        .logical = {},
    };
}
