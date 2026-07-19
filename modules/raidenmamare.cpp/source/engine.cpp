#include <rmmr/engine.h>

#include <base/logging.h>
#include <base/maybe.h>
#include <stdexcept>
#include <vector>

#include <rmmr/controller/camera.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/resources/shadows.q1.h>
#include <rmmr/system/core.q1.h>
#include <rmmr/system/imgui.q1.h>
#include <rmmr/system/interface.q1.h>
#include <rmmr/system/viewport.q1.h>

#include "renderer/renderer.h"

#include <GLFW/glfw3.h>

namespace rmmr {
    using namespace fqsm::api;

    namespace {

        Schema engineDomain() {
            static const Schema once = ask::schema::merge({
                ask::schema::aspect<system::Core>(),
                ask::schema::aspect<system::Device>(),
                ask::schema::aspect<system::ImGuiHost>(),
                ask::schema::aspect<system::Window>(),
                ask::schema::aspect<system::Viewport>(),
                ask::schema::aspect<system::Viewport_group>(),
                ask::schema::aspect<resource::Manager>(),
                ask::schema::aspect<resource::Unit>(),
                ask::schema::aspect<resource::Unit_group>(),
                ask::schema::aspect<resource::DeviceRuntimes>(),
                ask::schema::aspect<resource::Runtime_group>(),
                ask::schema::aspect<resource::ShaderRuntime_group>(),
                ask::schema::aspect<resource::MaterialRuntime_group>(),
                ask::schema::aspect<resource::ShadowRuntime_group>(),
                ask::schema::aspect<resource::GeometryRuntime_group>(),
                ask::schema::aspect<resource::Assets>(),
                ask::schema::aspect<resource::Runtimes>(),
                ask::schema::aspect<resource::texture::Asset>(),
                ask::schema::aspect<resource::texture::Loader>(),
                ask::schema::aspect<resource::texture::Generator>(),
                ask::schema::aspect<resource::texture::Runtime>(),
                ask::schema::aspect<resource::shader::Asset>(),
                ask::schema::aspect<resource::shader::Loader>(),
                ask::schema::aspect<resource::shader::Runtime>(),
                ask::schema::aspect<resource::material::Asset>(),
                ask::schema::aspect<resource::material::Composer>(),
                ask::schema::aspect<resource::material::Runtime>(),
                ask::schema::aspect<resource::shadow::Asset>(),
                ask::schema::aspect<resource::shadow::Allocator>(),
                ask::schema::aspect<resource::shadow::Runtime>(),
                ask::schema::aspect<resource::geometry::Asset>(),
                ask::schema::aspect<resource::geometry::Loader>(),
                ask::schema::aspect<resource::geometry::Generator>(),
                ask::schema::aspect<resource::geometry::Runtime>(),
                ask::schema::aspect<controller::Camera>(),
                ask::schema::aspect<scene::Root>(),
                ask::schema::aspect<scene::Node>(),
                ask::schema::aspect<scene::Node_group>(),
                ask::schema::aspect<scene::Camera>(),
                ask::schema::aspect<scene::Camera_group>(),
                ask::schema::aspect<scene::Light>(),
                ask::schema::aspect<scene::Light_group>(),
                ask::schema::aspect<scene::PrimitiveActor>(),
                ask::schema::aspect<scene::Grid>(),
            });
            return once;
        }

    } // namespace

    struct Engine::State : establish::Module::State {
        struct {
            maybe<system::Device::Id> device;
            maybe<system::Viewport::Id> viewport;
            maybe<scene::Root::Id> scene;
            maybe<scene::Camera::Id> scene_camera;
            maybe<resource::shadow::Asset::Id> default_shadow;
        } handles;
        Renderer renderer;

        explicit State(Schema schema)
            : establish::Module::State(std::move(schema))
        {}

        void setup(Writing context, establish::Module::RootId& root) {
            const auto core = root.secretGet<system::Core>();
            if (not core.exists())
                throw std::runtime_error("rmmr: RootId is not system::Core");
            if (handles.default_shadow.exists())
                return;
            using resource::Assets;
            using resource::Unit;
            handles.default_shadow = with<Assets>::add_shadow_allocator(
                context,
                *core,
                Unit::Quantum{.manager = *core, .name = "main_shadow", .library = "rmmr"},
                resource::shadow::Asset::Quantum{},
                resource::shadow::Allocator::Quantum{.size = index2{1024, 1024}});
        }

        void loadPastState(Writing) override {}
    };

    Engine::Engine() = default;
    Engine::~Engine() = default;

    Schema Engine::schema() {
        return engineDomain();
    }

    std::shared_ptr<establish::Module::State> Engine::installState(Schema finalSchema) {
        state = std::make_shared<State>(std::move(finalSchema));
        return state;
    }

    void Engine::setup(Writing context, establish::Module::RootId& root, WindowParameters params) {
        const auto core = root.secretGet<system::Core>();
        if (not core.exists())
            throw std::runtime_error("rmmr: RootId is not system::Core");

        base::message("rmmr: creating device and window...");
        state->handles.device = with<system::Interface>::addDeviceAndWindow(
            context,
            *core,
            std::move(params.title),
            params.requested_size);
        createViewport(context, with<system::Window>::framebufferSize(context, *state->handles.device));
        state->setup(context, root);
    }

    void Engine::materialize(Writing context, system::Core::Id assets) {
        with<resource::Runtimes>::materialize(context, *state->handles.device, assets);
    }

    void Engine::showScene(scene::Root::Id scene, scene::Camera::Id camera) {
        state->handles.scene = scene;
        state->handles.scene_camera = camera;
    }

    bool Engine::shouldClose(Reading context) const {
        return glfwWindowShouldClose(with<system::Device>::get(context, state->handles.device).handle);
    }

    void Engine::beginFrame(Writing context) {
        const auto& device = state->handles.device;
        const auto& viewport = state->handles.viewport;

        with<system::Device>::poll_events(context);
        with<system::Window>::onFrameAdvanced(context, device);

        {
            const auto& input = with<system::Window>::get(context, device).current;
            if (static_cast<std::size_t>(GLFW_KEY_ESCAPE) < input.keys.size()
                and input.keys[static_cast<std::size_t>(GLFW_KEY_ESCAPE)])
            {
                glfwSetWindowShouldClose(with<system::Device>::get(context, device).handle, true);
            }
        }

        if (state->handles.scene_camera) {
            with<controller::Camera>::update(context, state->handles.scene_camera, device);
        }

        with<system::Viewport>::syncExtent(context, viewport);
        with<system::Viewport>::activate(context, viewport);
        with<system::Viewport>::clear(context, viewport);

        /* natural perfomance test, keep this as comment please
        for (int xx = 0; xx < 100; ++xx)
            with<system::Viewport>::modify(context, viewport)->clear_color.r = 0;
        */

        if (state->handles.scene && state->handles.scene_camera) {
            with<system::ImGuiHost>::newFrame(context, device);
        }
    }

    void Engine::render(Writing context) {
        if (not state->handles.scene or not state->handles.scene_camera)
            return;
        state->renderer.render(Renderer::FrameContext{
            .world = context,
            .viewport = *state->handles.viewport,
            .window = *state->handles.device,
            .scene = *state->handles.scene,
            .camera = *state->handles.scene_camera,
        });
    }

    void Engine::endFrame(Writing context) {
        const auto& device = state->handles.device;
        if (state->handles.scene && state->handles.scene_camera) {
            with<system::ImGuiHost>::render(context, device);
        }
        with<system::Window>::present(context, device);
    }

    void Engine::createViewport(Writing context, index2 size, index2 origin) {
        state->handles.viewport = with<system::Viewport_group>::addElement(context, *state->handles.device, system::Viewport::Quantum{
            .origin = origin,
            .size = size,
            .clear_color = vec4{0.2f, 0.3f, 0.3f, 1.0f},
        });
    }

    void Engine::shutdown(Writing context) noexcept {
        base::message("rmmr teardown: Engine shutdown begin");
        std::vector<GLFWwindow*> preserved_handles;
        for (const auto entry : context->aspect<system::Device>().items()) {
            if (entry.value.handle) {
                preserved_handles.push_back(entry.value.handle);
            }
        }
        ask::temp_sugar::drop_reference<system::Viewport>(context, state->handles.viewport);
        with<system::Interface>::shutdown(context);
        for (GLFWwindow* handle : preserved_handles) {
            glfwDestroyWindow(handle);
        }
        glfwTerminate();
        base::message("rmmr teardown: Engine shutdown done");
    }
}
