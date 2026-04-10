#include <Raidenmamare/engine.h>

#include <Raidenmamare/core.q1.h>
#include <Raidenmamare/program.q1.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <iostream>

namespace {
} // namespace

namespace rmmr {

Engine::Engine(std::string assetsRoot)
    : assetsRoot(std::move(assetsRoot))
    , state(ops::world::create(resourceAspects()))
    , resourceManager(base::make_shared<iqsm::resources::ManagerCore>(resourceAspects()))
    , core(ops::resource::declare<Core>(
        state,
        Core::Quantum{
            .passport = Core::Materializer::Passport{
                .assets_root = this->assetsRoot,
                .title = "Raidenmamare",
                .width = 800,
                .height = 600,
                .context_major = 3,
                .context_minor = 3,
            },
        }
    ))
{}

iqsm::Schema Engine::resourceAspects() {
    return ops::schema::assemble<Core, Program>();
}

int Engine::run_render_demo() {
    try {
        Core::Operations::open(state, core, resourceManager);
    } catch (const std::exception& e) {
        std::cerr << "Failed to open Core resource: " << e.what() << std::endl;
        return 1;
    }

    GLFWwindow* window = Core::Operations::provide(state, core, resourceManager);

    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "Press ESC to exit." << std::endl;

    const Program::Id program = ops::resource::declare<Program>(
        state,
        Program::Quantum{
            .passport = Program::Materializer::Passport{
                .debugName = "triangle",
                .vertexFilename = "shaders/triangle.vert.glsl",
                .fragmentFilename = "shaders/triangle.frag.glsl",
            },
            .core = core,
        }
    );
    try {
        Program::Operations::open(state, program, resourceManager);
    } catch (const std::exception& e) {
        std::cerr << "Failed to open Program resource: " << e.what() << std::endl;
        Core::Operations::close(state, core, resourceManager);
        return 3;
    }

    const GLuint shaderProgram = Program::Operations::provide(state, program, resourceManager);
    if (!shaderProgram) {
        std::cerr << "Failed to access Program resource" << std::endl;
        Program::Operations::close(state, program, resourceManager);
        Core::Operations::close(state, core, resourceManager);
        return 3;
    }


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
    Program::Operations::close(state, program, resourceManager);
    Core::Operations::close(state, core, resourceManager);
    return 0;
}

} // namespace rmmr

