#include <Raidenmamare/engine.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <base/maybe.h>
#include <iQSM/logger.h>
#include <stdexcept>

#include <Raidenmamare/core.q1.h>
#include <Raidenmamare/primitives/base.q1.h>
#include <Raidenmamare/materials/program.q1.h>
#include <Raidenmamare/materials/type.q1.h>
#include <Raidenmamare/viewport.q1.h>

using namespace iqsm::dsl_gateway;

namespace rmmr::internal {

    struct EngineState {
        iqsm::repo::Branch main;
        iqsm::dsl_gateway::resources::Manager resourceManager;

        // this objects may vary a lot with Engine develops in time
        base::maybe<Core::Id> core;
        base::maybe<material::Type::Id> materialType;
        base::maybe<primitive::Base::Id> primitive;
        base::maybe<Viewport::Id> viewport;
    };
}

namespace rmmr {

Engine::Engine(StartupParameters params) {
    const auto schema = resourceAspects();
    state = std::make_shared<State>(State{
        .main = iqsm::repo::Branch(ops::world::create(schema)),
        .resourceManager = base::make_shared<iqsm::resources::ManagerCore>(schema),
        .core = {},
        .materialType = {},
        .primitive = {},
        .viewport = {},
    });

    state->core = ops::resource::declare<Core>(
        state->main,
        Core::Quantum{
            .passport = std::move(params),
        });

    using namespace iqsm::logger;
    const auto& corePassport = ops::particle::get<Core>(state->main, state->core).passport;
    message("Engine initialized for '{}'", corePassport.title);
    message("Requested OpenGL context {}.{}", corePassport.context_major, corePassport.context_minor);

    message("Loading resources...");
    prepareResources();
    message("... loading resources: Done");
}

Engine::~Engine() noexcept {
    shutdown();
}

iqsm::Schema Engine::resourceAspects() {
    return ops::schema::assemble<Core, material::Program, material::Type, primitive::Base, Viewport>();
}

void Engine::shutdown() noexcept {
    try {
        state->resourceManager->shutdown(state->main);
    } catch (...) {
    }
}


void Engine::prepareResources() {
    auto& main = state->main;
    auto resourceManager = state->resourceManager;
    const auto core = state->core;

    Core::Operations::materialize(main, core, resourceManager);
    const auto& corePassport = ops::particle::get<Core>(main, core).passport;

    // hardcoded for now...
    // this Program::Id will be saved in state via material::Type later...
    const material::Program::Id program = ops::resource::declare<material::Program>(
        main,
        material::Program::Quantum{
            .passport = material::Program::Materializer::Passport{
                .debugName = "triangle",
                .vertexFilename = "shaders/triangle.vert.glsl",
                .fragmentFilename = "shaders/triangle.frag.glsl",
            },
            .core = core,
        }
    );

    material::Program::Operations::materialize(main, program, resourceManager);

    state->materialType = ops::resource::declare<material::Type>(
        main,
        material::Type::Quantum{
            .passport = material::Type::Materializer::Passport{
                .program = program,
            },
            .name = "triangle",
        }
    );
    ops::resource::materialize<material::Type>(main, resourceManager, state->materialType);

    const auto& materialTypeRuntime = resourceManager->layer<material::Type>().provide(state->materialType);
    if (!materialTypeRuntime.program) {
        throw std::runtime_error("Engine::prepareResources: material::Type runtime program is null");
    }

    state->primitive = ops::resource::declare<primitive::Base>(
        main,
        primitive::Base::Quantum{
            .passport = primitive::Base::Materializer::Passport{
                .debugName = "triangle",
            },
            .core = core,
            .vertices = vector<vec3>{
                vec3{-0.5f, -0.5f, 0.0f},
                vec3{ 0.5f, -0.5f, 0.0f},
                vec3{ 0.0f,  0.5f, 0.0f},
            },
        }
    );
    primitive::Base::Operations::bake(main, state->primitive, resourceManager);

    state->viewport = ops::particle::create<Viewport>(
        main,
        Viewport::Quantum{
            .core = core,
            .origin = index2{.x = 0, .y = 0},
            .size = corePassport.size,
            .clear_color = vec4{0.2f, 0.3f, 0.3f, 1.0f},
        }
    );
}


int Engine::run_render_demo() {
    const auto core = state->core;
    const auto materialType = state->materialType;
    const auto primitive = state->primitive;
    const auto viewport = state->viewport;
    auto& main = state->main;
    auto resourceManager = state->resourceManager;

    GLFWwindow* window = Core::Operations::provide(main, core, resourceManager);
    const auto& materialTypeRuntime = resourceManager->layer<material::Type>().provide(materialType);
    const auto shaderProgram = materialTypeRuntime.program;
    if (!shaderProgram) {
        throw std::runtime_error("Engine::run_render_demo: material::Type runtime program is null");
    }
    const auto& primitive_runtime = primitive::Base::Operations::provide(main, primitive, resourceManager);

    while (!glfwWindowShouldClose(window)) {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, true);
        }

        Viewport::Operations::activate(main, viewport, resourceManager);
        Viewport::Operations::clear(main, viewport, resourceManager);

        glUseProgram(shaderProgram);
        glBindVertexArray(primitive_runtime.vao);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(primitive_runtime.vertex_count));

        Core::Operations::present(main, core, resourceManager);
        Core::Operations::poll_events(main, core, resourceManager);
    }

    shutdown();
    return 0;
}

} // namespace rmmr

