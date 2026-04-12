#include <Raidenmamare/engine.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/type_ptr.hpp>
#include <base/maybe.h>
#include <iQSM/logger.h>
#include <random>

#include <Raidenmamare/device.q1.h>
#include <Raidenmamare/primitives/base.q1.h>
#include <Raidenmamare/viewport.q1.h>
#include <Raidenmamare/scene/core.q1.h>
#include <Raidenmamare/scene/actor.q1.h>
#include <Raidenmamare/scene/camera.q1.h>
#include <Raidenmamare/scene/light.q1.h>
#include <Raidenmamare/math.q1.h>

// private stuff:
#include "materials/materialGenerator.h"
#include "primitives/meshGenerator.h"
#include "renderer/renderer.h"

using namespace iqsm::dsl_gateway;

namespace rmmr::internal {

    struct EngineState {
        iqsm::repo::Branch main;
        iqsm::dsl_gateway::resources::Manager resourceManager;
        maybe<rmmr::Renderer> renderer;

        // this objects may vary a lot with Engine develops in time
        maybe<Device::Id> device;
        maybe<material::Core::Id> materialAmbient;
        maybe<material::Core::Id> materialLit;
        maybe<Viewport::Id> viewport; // TODO: remove this link later
        maybe<primitive::Base::Id> primitive;
        maybe<primitive::Base::Id> primitiveKube;
        maybe<scene::Core::Id> scene;
    };

    static auto viewport_aspect_ratio(Reading world, Viewport::Id viewport) -> float {
        const auto& quantum = ops::particle::get<Viewport>(world, viewport);
        const float width = quantum.size.x > integer{0} ? static_cast<float>(quantum.size.x) : 1.0f;
        const float height = quantum.size.y > integer{0} ? static_cast<float>(quantum.size.y) : 1.0f;
        return width / height;
    }
}

namespace rmmr {

Engine::Engine(StartupParameters params) {
    const auto schema = resourceAspects();
    state = std::make_shared<State>(State{
        .main = iqsm::repo::Branch(ops::world::create(schema)),
        .resourceManager = base::make_shared<iqsm::resources::ManagerCore>(schema),
        .renderer = {},
        .device = {},
        .materialAmbient = {},
        .materialLit = {},
        .viewport = {},
        .primitive = {},
        .primitiveKube = {},
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

    state->renderer = rmmr::Renderer(state->resourceManager.std_ptr(), state->device);
}

Engine::~Engine() noexcept {
    shutdown();
}

iqsm::Schema Engine::resourceAspects() {
    return ops::schema::assemble<
        Device,
        material::Program,
        material::Core,
        primitive::Base,
        Viewport,
        scene::Node,
        scene::PrimitiveActor,
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

    state->materialAmbient = material::MaterialGenerator::ambient(main, device, resourceManager);
    state->materialLit = material::MaterialGenerator::lit(main, device, resourceManager);

    state->primitive = primitive::MeshGenerator::triangle(main, device, resourceManager);
    state->primitiveKube = primitive::MeshGenerator::kube(main, device, resourceManager);

    createViewport(devicePassport.size);
    createScene();
}


void Engine::createScene() {
    repo::Sequence transaction{state->main};

    state->scene = ops::particle::create<scene::Core>(
        transaction,
        scene::Core::Quantum{
            .nodes = {},
            .ambient = RGB{0.4f, 0.4f, 0.4f},
            .ambient_intensity = 0.8f,
        });

    for (int i = 0; i < 5; ++i) {
        ops::particle::modifier<scene::Core>(transaction, state->scene)->nodes.push_back(
            scene::PrimitiveActor::Operations::create(
                transaction,
                Pos{-1.4f + 0.7f * static_cast<float>(i), 0.5f, 0.0f},
                HPB{0.0f, 0.0f, 0.0f},
                state->primitive,
                state->materialAmbient,
                RGB{1.0f - 0.15f * static_cast<float>(i), 0.5f, 0.2f + 0.15f * static_cast<float>(i)}
            )
        );
    }

    std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> heading_deg{-180.0f, 180.0f};
    std::uniform_real_distribution<float> pitch_deg{-45.0f, 45.0f};
    std::uniform_real_distribution<float> bank_deg{-45.0f, 45.0f};

    for (int i = 0; i < 4; ++i) {
        ops::particle::modifier<scene::Core>(transaction, state->scene)->nodes.push_back(
            scene::PrimitiveActor::Operations::create(
                transaction,
                Pos{-1.05f + 0.7f * static_cast<float>(i), 0.2f, 0.0f},
                HPB{heading_deg(rng), pitch_deg(rng), bank_deg(rng)},
                state->primitiveKube,
                state->materialLit,
                RGB{0.2f + 0.2f * static_cast<float>(i), 0.45f, 1.0f - 0.2f * static_cast<float>(i)}
            )
        );
    }

    // Placeholder camera: lives on its own node and targets the current viewport.
    // Rough framing: a bit farther (~2× previous z), higher (~1.3), pitch down toward mid of triangles (y≈0.5) + cubes (y≈0.2).
    ops::particle::modifier<scene::Core>(transaction, state->scene)->nodes.push_back(
        scene::Camera::Operations::create(
            transaction,
            Pos{0.0f, 1.3f, 4.0f},
            HPB{0.0f, -12.5f, 0.0f},
            1.04719755f, // ~60 degrees in radians
            0.1f,
            100.0f
        )
    );

    // Placeholder point light: also a node payload.
    ops::particle::modifier<scene::Core>(transaction, state->scene)->nodes.push_back(scene::Light::Operations::create(
            transaction,
            Pos{2.0f, 2.0f, 2.0f},
            HPB{0.0f, 0.0f, 0.0f},
            RGB{1.0f, 1.0f, 1.0f},
            5.0f,
            10.0f
        )
    );

    state->main.absorb(transaction.push());
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
    auto& main = state->main;
    auto resourceManager = state->resourceManager;

    GLFWwindow* window = Device::Operations::provide(main, device, resourceManager);


    while (!glfwWindowShouldClose(window)) {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, true);
        }

        state->renderer->render_new_temp({main, state->viewport, state->scene});

        Device::Operations::present(main, device, resourceManager);
        Device::Operations::poll_events(main, device, resourceManager);
    }

    shutdown();
    return 0;
}

} // namespace rmmr

