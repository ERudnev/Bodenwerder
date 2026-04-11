#include <Raidenmamare/engine.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <base/maybe.h>
#include <iQSM/logger.h>
#include <stdexcept>

#include <Raidenmamare/device.q1.h>
#include <Raidenmamare/primitives/base.q1.h>
#include <Raidenmamare/materials/program.q1.h>
#include <Raidenmamare/materials/type.q1.h>
#include <Raidenmamare/viewport.q1.h>
#include <Raidenmamare/scene/core.q1.h>
#include <Raidenmamare/scene/camera.q1.h>
#include <Raidenmamare/scene/light.q1.h>
#include <Raidenmamare/math.q1.h>

using namespace iqsm::dsl_gateway;

namespace rmmr::internal {

    struct EngineState {
        iqsm::repo::Branch main;
        iqsm::dsl_gateway::resources::Manager resourceManager;

        // this objects may vary a lot with Engine develops in time
        maybe<Device::Id> device;
        maybe<material::Type::Id> materialType;
        maybe<primitive::Base::Id> primitive;
        maybe<Viewport::Id> viewport; // TODO: remove this link later
        maybe<scene::Core::Id> scene;
    };
}

namespace rmmr {

Engine::Engine(StartupParameters params) {
    const auto schema = resourceAspects();
    state = std::make_shared<State>(State{
        .main = iqsm::repo::Branch(ops::world::create(schema)),
        .resourceManager = base::make_shared<iqsm::resources::ManagerCore>(schema),
        .device = {},
        .materialType = {},
        .primitive = {},
        .viewport = {},
        .scene = {},
    });

    state->device = ops::resource::declare<Device>(
        state->main,
        Device::Quantum{
            .passport = std::move(params),
        });

    using namespace iqsm::logger;
    const auto& devicePassport = ops::particle::get<Device>(state->main, state->device).passport;
    message("Engine initialized for '{}'", devicePassport.title);
    message("Requested OpenGL context {}.{}", devicePassport.context_major, devicePassport.context_minor);

    message("Loading resources...");
    prepareResources();
    message("... loading resources: Done");
}

Engine::~Engine() noexcept {
    shutdown();
}

iqsm::Schema Engine::resourceAspects() {
    return ops::schema::assemble<
        Device,
        material::Program,
        material::Type,
        primitive::Base,
        Viewport,
        scene::Node,
        scene::Camera,
        scene::Light,
        scene::Core>();
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
    const auto device = state->device;

    Device::Operations::materialize(main, device, resourceManager);
    const auto& devicePassport = ops::particle::get<Device>(main, device).passport;

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
            .device = device,
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
            .device = device,
            .vertices = vector<vec3>{
                vec3{-0.5f, -0.5f, 0.0f},
                vec3{ 0.5f, -0.5f, 0.0f},
                vec3{ 0.0f,  0.5f, 0.0f},
            },
        }
    );
    primitive::Base::Operations::bake(main, state->primitive, resourceManager);

    createViewport(devicePassport.size);
    createScene();
}


void Engine::createScene() {
    auto& main = state->main;

    // Single root node for the demo: slightly above origin, identity rotation (triangle will sit in this space later).
    const auto demoRoot = ops::particle::create<scene::Node>(
        main,
        scene::Node::Quantum{
            .position = vec3{0.0f, 0.5f, 0.0f},
            .rotation = quat{1.0f, 0.0f, 0.0f, 0.0f},
        });

    // Placeholder camera: lives on its own node and targets the current viewport.
    const auto cameraNode = scene::Camera::Operations::create(
        main,
        Pos{0.0f, 0.0f, 2.0f},
        HPB{0.0f, 0.0f, 0.0f},
        1.04719755f, // ~60 degrees in radians
        0.1f,
        100.0f
    );

    // Placeholder point light: also a node payload.
    const auto lightNode = scene::Light::Operations::create(
        main,
        Pos{2.0f, 2.0f, 2.0f},
        HPB{0.0f, 0.0f, 0.0f},
        RGB{1.0f, 1.0f, 1.0f},
        5.0f,
        10.0f
    );

    state->scene = ops::particle::create<scene::Core>(
        main,
        scene::Core::Quantum{
            .nodes = vector<scene::Node::Id>{ demoRoot, cameraNode, lightNode },
        });
}


void Engine::createViewport(index2 size, index2 origin) {
    state->viewport = ops::particle::create<Viewport>(
        state->main,
        Viewport::Quantum{
            .device = state->device,
            .origin = origin,
            .size = size,
            .clear_color = vec4{0.2f, 0.3f, 0.3f, 1.0f},
        }
    );
}


int Engine::run_render_demo() {
    const auto device = state->device;
    const auto materialType = state->materialType;
    const auto primitive = state->primitive;
    const auto viewport = state->viewport;
    auto& main = state->main;
    auto resourceManager = state->resourceManager;

    GLFWwindow* window = Device::Operations::provide(main, device, resourceManager);
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

        Device::Operations::present(main, device, resourceManager);
        Device::Operations::poll_events(main, device, resourceManager);
    }

    shutdown();
    return 0;
}

} // namespace rmmr

