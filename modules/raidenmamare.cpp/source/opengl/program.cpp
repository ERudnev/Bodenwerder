#include <opengl/program.h>

#include <GL/glew.h>

#include <base/logging.h>

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>

namespace {
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
        if (assetRoot.empty()) _THROW_LOGIC_ERROR_;
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
}

namespace rmmr::opengl {
    Program::Program(GLuint handle)
        : handle(handle) {}

    Program::~Program() {
        if (!handle) return;
        glDeleteProgram(handle);
    }

    auto Program::create(const std::filesystem::path& assetRoot, const rmmr::Program::Passport& passport) -> Ptr {
        const auto vertexPath = resolveProgramPath(assetRoot, passport.vertexFilename);
        const auto fragmentPath = resolveProgramPath(assetRoot, passport.fragmentFilename);

        const std::string vertexSource = readTextFile(vertexPath);
        if (vertexSource.empty()) return {};

        const std::string fragmentSource = readTextFile(fragmentPath);
        if (fragmentSource.empty()) return {};

        const GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource, "vertex shader");
        if (!vertexShader) return {};

        const GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource, "fragment shader");
        if (!fragmentShader) {
            glDeleteShader(vertexShader);
            return {};
        }

        const GLuint program = glCreateProgram();
        glAttachShader(program, vertexShader);
        glAttachShader(program, fragmentShader);

        const bool linked = linkProgram(program);
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        if (!linked) {
            glDeleteProgram(program);
            return {};
        }

        return std::make_unique<Program>(program);
    }

    auto Program::getHandle(Provider provider, rmmr::Program::Id id) -> GLuint {
        const auto* program = dynamic_cast<const Program*>(provider->layer<rmmr::Program>()->get(id));
        if (!program) _THROW_LOGIC_ERROR_;
        return program->handle;
    }
}
