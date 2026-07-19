#include <rmmr/engine.h>

#include <base/logging.h>
#include <base/maybe.h>
#include <stdexcept>
#include <vector>

#include <rmmr/controller/camera.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/system/core.q1.h>
#include <rmmr/system/imgui.q1.h>
#include <rmmr/system/interface.q1.h>
#include <rmmr/system/viewport.q1.h>

#include "renderer/renderer.h"
#include "ui/overlay.h"

#include <GLFW/glfw3.h>

namespace rmmr {
    using namespace fqsm::api;

    namespace {

        Schema engineDomain() {
            return ask::schema::merge({
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
        }

    } // namespace

    struct Engine::State : establish::Module::State {
        Engine* engine;
        maybe<system::Device::Id> device;
        maybe<system::Viewport::Id> viewport;
        maybe<scene::Root::Id> scene;
        maybe<scene::Camera::Id> scene_camera;
        Renderer renderer;
        bool show_materials = false;

        State(Schema schema, Engine* owner)
            : establish::Module::State(std::move(schema))
            , engine(owner)
        {}

        void createDefaultState(Writing context, establish::Module::RootId& root) override {
            const auto core = root.secretGet<system::Core>();
            if (not core.exists())
                throw std::runtime_error("rmmr: RootId is not system::Core");
            engine->open(context, *core, engine->window);
        }

        void loadPastState(Writing) override {}
    };

    Engine::Engine() = default;
    Engine::~Engine() = default;

    Schema Engine::domain() {
        return engineDomain();
    }

    std::shared_ptr<establish::Module::State> Engine::installState(Schema finalSchema) {
        state = std::make_shared<State>(std::move(finalSchema), this);
        return state;
    }

    void Engine::setWindowParameters(WindowParameters params) {
        window = std::move(params);
    }

    void Engine::open(Writing context, system::Core::Id core, WindowParameters params) {
        base::message("rmmr: creating device and window...");
        state->device = with<system::Interface>::addDeviceAndWindow(
            context,
            core,
            std::move(params.title),
            params.requested_size);
        createViewport(context, with<system::Window>::framebufferSize(context, *state->device));
    }

    void Engine::materialize(Writing context, system::Core::Id assets) {
        with<resource::Runtimes>::materialize(context, *state->device, assets);
    }

    void Engine::setScene(scene::Root::Id scene, scene::Camera::Id camera) {
        state->scene = scene;
        state->scene_camera = camera;
    }

    bool Engine::shouldClose(Reading context) const {
        return glfwWindowShouldClose(with<system::Device>::get(context, state->device).handle);
    }

    void Engine::frame(Writing context) {
        const auto& device = state->device;
        const auto& viewport = state->viewport;

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

        if (state->scene_camera) {
            with<controller::Camera>::update(context, state->scene_camera, device);
        }

        with<system::Viewport>::syncExtent(context, viewport);
        with<system::Viewport>::activate(context, viewport);
        with<system::Viewport>::clear(context, viewport);

        /* natural perfomance test, keep this as comment please
        for (int xx = 0; xx < 100; ++xx)
            with<system::Viewport>::modify(context, viewport)->clear_color.r = 0;
        */

        if (state->scene && state->scene_camera) {
            with<system::ImGuiHost>::newFrame(context, device);
            const ui::FrameContext ui_frame{
                .world = context,
                .window = *device,
                .scene = *state->scene,
                .camera = *state->scene_camera,
                .show_materials = state->show_materials,
            };
            ui::draw_renderer_toggles(ui_frame);
            ui::draw_camera(ui_frame);
            if (state->show_materials) ui::draw_materials(ui_frame);
            state->renderer.render(Renderer::FrameContext{
                .world = context,
                .viewport = *viewport,
                .window = *device,
                .scene = *state->scene,
                .camera = *state->scene_camera,
            });
            with<system::ImGuiHost>::render(context, device);
        }

        with<system::Window>::present(context, device);
    }

    void Engine::createViewport(Writing context, index2 size, index2 origin) {
        state->viewport = with<system::Viewport_group>::addElement(context, *state->device, system::Viewport::Quantum{
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
        ask::temp_sugar::drop_reference<system::Viewport>(context, state->viewport);
        with<system::Interface>::shutdown(context);
        for (GLFWwindow* handle : preserved_handles) {
            glfwDestroyWindow(handle);
        }
        glfwTerminate();
        base::message("rmmr teardown: Engine shutdown done");
    }
}
