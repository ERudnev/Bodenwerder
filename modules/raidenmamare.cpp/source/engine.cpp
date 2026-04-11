#include <Raidenmamare/engine.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/type_ptr.hpp>
#include <base/maybe.h>
#include <iQSM/logger.h>
#include <stdexcept>

#include <Raidenmamare/device.q1.h>
#include <Raidenmamare/primitives/base.q1.h>
#include <Raidenmamare/materials/program.q1.h>
#include <Raidenmamare/materials/core.q1.h>
#include <Raidenmamare/viewport.q1.h>
#include <Raidenmamare/scene/core.q1.h>
#include <Raidenmamare/scene/actor.q1.h>
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
        maybe<material::Core::Id> materialCore;
        maybe<primitive::Base::Id> primitive;
        maybe<Viewport::Id> viewport; // TODO: remove this link later
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
        .device = {},
        .materialCore = {},
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

    // hardcoded for now...
    // this Program::Id will be saved in state via material::Core later...
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

    state->materialCore = ops::resource::declare<material::Core>(
        main,
        material::Core::Quantum{
            .passport = material::Core::Materializer::Passport{
                .program = program,
                .uniforms = material::Core::Operations::uniformIds(vector<string>{
                    "model",
                    "view",
                    "projection",
                    "albedo",
                    "lightColor",
                    "lightIntensity",
                }),
            },
            .name = "triangle",
        }
    );
    ops::resource::materialize<material::Core>(main, resourceManager, state->materialCore);

    const auto& materialCoreRuntime = resourceManager->layer<material::Core>().provide(state->materialCore);
    if (!materialCoreRuntime.program) {
        throw std::runtime_error("Engine::prepareResources: material::Core runtime program is null");
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

    const auto actorNode = scene::PrimitiveActor::Operations::create(
        main,
        Pos{0.0f, 0.5f, 0.0f},
        HPB{0.0f, 0.0f, 0.0f},
        state->primitive,
        state->materialCore
    );

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
            .nodes = vector<scene::Node::Id>{ actorNode, cameraNode, lightNode },
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

void render(const internal::EngineState& state) {
    auto& main = state.main;
    auto resourceManager = state.resourceManager;
    const auto viewport = state.viewport;
    const auto scene_id = state.scene;

    const auto& scene_core = ops::particle::get<scene::Core>(main, scene_id);
    if (scene_core.cameras.empty()) throw std::runtime_error("Engine::run_render_demo: scene has no camera node");
    if (scene_core.lights.empty()) throw std::runtime_error("Engine::run_render_demo: scene has no light node");

    const auto camera = scene_core.cameras.front();
    const auto light_id = scene_core.lights.front();
    const float aspect_ratio = internal::viewport_aspect_ratio(main, viewport);
    const mat4 view = scene::Camera::Operations::view(main, camera);
    const mat4 projection = scene::Camera::Operations::projection(main, camera, aspect_ratio);
    const auto& light = ops::particle::get<scene::Light>(main, light_id);

    Viewport::Operations::activate(main, viewport, resourceManager);
    Viewport::Operations::clear(main, viewport, resourceManager);

    for (const auto node : scene_core.nodes) {
        if (!ops::particle::exists<scene::PrimitiveActor>(main, node)) continue;

        const auto& actor = ops::particle::get<scene::PrimitiveActor>(main, node);
        const auto& material_core_runtime = resourceManager->layer<material::Core>().provide(actor.material);
        const auto shaderProgram = material_core_runtime.program;
        if (!shaderProgram) {
            throw std::runtime_error("Engine::run_render_demo: actor material runtime program is null");
        }

        const auto& primitive_runtime = primitive::Base::Operations::provide(main, actor.geometry, resourceManager);
        const GLint model_location = glGetUniformLocation(shaderProgram, "u_model");
        const GLint view_location = glGetUniformLocation(shaderProgram, "u_view");
        const GLint projection_location = glGetUniformLocation(shaderProgram, "u_projection");
        const GLint albedo_location = glGetUniformLocation(shaderProgram, "u_albedo");
        const GLint light_color_location = glGetUniformLocation(shaderProgram, "u_lightColor");
        const GLint light_intensity_location = glGetUniformLocation(shaderProgram, "u_lightIntensity");
        const mat4 model = scene::Node::Operations::transform(main, node);

        glUseProgram(shaderProgram);
        glUniformMatrix4fv(model_location, 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(view_location, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projection_location, 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3f(albedo_location, 1.0f, 0.5f, 0.2f);
        glUniform3f(light_color_location, light.color.x, light.color.y, light.color.z);
        glUniform1f(light_intensity_location, light.intensity);
        glBindVertexArray(primitive_runtime.vao);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(primitive_runtime.vertex_count));
    }
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

        render(*state);

        Device::Operations::present(main, device, resourceManager);
        Device::Operations::poll_events(main, device, resourceManager);
    }

    shutdown();
    return 0;
}

} // namespace rmmr

