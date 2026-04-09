#include <Raidenmamare/engine.h>

#include <Raidenmamare/core.q1.h>
#include <Raidenmamare/program.q1.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <iostream>

namespace {
    const char* kVertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;

void main() {
    gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);
}
)";

    const char* kFragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

void main() {
    FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f); // Orange color
}
)";

    bool compile_shader(GLuint shader, const char* label) {
        glCompileShader(shader);
        int success = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (success) return true;

        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << label << " compilation failed: " << infoLog << std::endl;
        return false;
    }

    bool link_program(GLuint program) {
        glLinkProgram(program);
        int success = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (success) return true;

        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "Shader program linking failed: " << infoLog << std::endl;
        return false;
    }

} // namespace

namespace rmmr {
    auto coreLoader() -> const iqsm::binding::resource::Loader<Core>&;
    auto getWindow(iqsm::binding::resource::Manager manager, Core::Id id) -> GLFWwindow*;
}

namespace rmmr {

Engine::Engine(std::string assetsRoot)
    : assetsRoot(std::move(assetsRoot))
    , state(ops::world::create(resourceAspects()))
    , resourceManager(base::make_shared<iqsm::binding::ManagerData>())
    , core(ops::resource::declare<Core>(
        state,
        Core::Quantum{
            .passport = Core::Passport{
                .assets_root = this->assetsRoot,
            },
        }
    ))
{}

iqsm::Schema Engine::resourceAspects() {
    return ops::schema::assemble<Core, Program>();
}

int Engine::run_render_demo() {
    if (!ops::resource::load<Core>(state, resourceManager, core, coreLoader())) {
        std::cerr << "Failed to load Core resource" << std::endl;
        return 1;
    }

    GLFWwindow* window = getWindow(resourceManager, core);
    if (!window) {
        std::cerr << "Failed to acquire GLFW window from Core resource" << std::endl;
        ops::resource::unload<Core>(state, resourceManager, core, coreLoader());
        return 2;
    }

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        ops::resource::unload<Core>(state, resourceManager, core, coreLoader());
        return 3;
    }

    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "Press ESC to exit." << std::endl;

    float vertices[] = {
        -0.5f, -0.5f, 0.0f,
        0.5f,  -0.5f, 0.0f,
        0.0f,  0.5f,  0.0f
    };

    GLuint VBO = 0, VAO = 0;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &kVertexShaderSource, nullptr);
    if (!compile_shader(vertexShader, "Vertex shader")) {
        ops::resource::unload<Core>(state, resourceManager, core, coreLoader());
        return 4;
    }

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &kFragmentShaderSource, nullptr);
    if (!compile_shader(fragmentShader, "Fragment shader")) {
        glDeleteShader(vertexShader);
        ops::resource::unload<Core>(state, resourceManager, core, coreLoader());
        return 5;
    }

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    if (!link_program(shaderProgram)) {
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        glDeleteProgram(shaderProgram);
        ops::resource::unload<Core>(state, resourceManager, core, coreLoader());
        return 6;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);

    while (!glfwWindowShouldClose(window)) {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, true);
        }

        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shaderProgram);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);
    ops::resource::unload<Core>(state, resourceManager, core, coreLoader());
    return 0;
}

} // namespace rmmr

