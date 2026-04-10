#include <Raidenmamare/engine.h>

#include <Raidenmamare/core.q1.h>
#include <Raidenmamare/program.q1.h>

#include <opengl/context.h>
#include <opengl/program.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <iostream>

namespace {
} // namespace

namespace rmmr {
    auto coreLoader() -> const iqsm::binding::resource::Loader<Core>&;
    auto programLoader() -> const iqsm::binding::resource::Loader<Program>&;
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

    GLFWwindow* window = opengl::Context::getWindow(resourceManager, core);

    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "Press ESC to exit." << std::endl;

    const Program::Id program = ops::resource::declare<Program>(
        state,
        Program::Quantum{
            .passport = Program::Passport{
                .debugName = "triangle",
                .vertexFilename = "shaders/triangle.vert.glsl",
                .fragmentFilename = "shaders/triangle.frag.glsl",
            },
            .core = core,
        }
    );
    if (!ops::resource::load<Program>(state, resourceManager, program, programLoader())) {
        std::cerr << "Failed to load Program resource" << std::endl;
        ops::resource::unload<Core>(state, resourceManager, core, coreLoader());
        return 3;
    }

    const GLuint shaderProgram = opengl::Program::getHandle(resourceManager, program);

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
    ops::resource::unload<Program>(state, resourceManager, program, programLoader());
    ops::resource::unload<Core>(state, resourceManager, core, coreLoader());
    return 0;
}

} // namespace rmmr

