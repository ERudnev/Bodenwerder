#include <Raidenmamare/engine.h>

#include <Raidenmamare/core.q1.h>
#include <Raidenmamare/program.q1.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <iQSM/logger.h>

namespace {
} // namespace

namespace rmmr {

Engine::Engine(Passport passport)
    : passport(std::move(passport))
    , state(ops::world::create(resourceAspects()))
    , resourceManager(base::make_shared<iqsm::resources::ManagerCore>(resourceAspects()))
    , core(ops::resource::declare<Core>(
        state,
        Core::Quantum{
            .passport = this->passport.core,
        }
    ))
{
    using namespace iqsm::logger;
    message("Engine initialized for '{}'", this->passport.core.title);
    message("Requested OpenGL context {}.{}", this->passport.core.context_major, this->passport.core.context_minor);
}

Engine::~Engine() noexcept {
    shutdown();
}

iqsm::Schema Engine::resourceAspects() {
    return ops::schema::assemble<Core, Program>();
}

void Engine::shutdown() noexcept {
    try {
        resourceManager->shutdown(state);
    } catch (...) {
    }
}

int Engine::run_render_demo() {
    Core::Operations::open(state, core, resourceManager);

    GLFWwindow* window = Core::Operations::provide(state, core, resourceManager);

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
    Program::Operations::open(state, program, resourceManager);

    const GLuint shaderProgram = Program::Operations::provide(state, program, resourceManager);
    if (!shaderProgram) {
        throw std::runtime_error("Engine::run_render_demo: Program::provide returned 0");
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
    shutdown();
    return 0;
}

} // namespace rmmr

