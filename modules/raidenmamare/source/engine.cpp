#include <rmmr/engine.h>

#include <base/logging.h>
#include <base/maybe.h>
#include <stdexcept>
#include <vector>

#include <rmmr/controller/camera2d.q1.h>
#include <rmmr/controller/camera3d.q1.h>
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
                ask::schema::aspect<system::Clock>(),
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
                ask::schema::aspect<resource::SpriteRuntime_group>(),
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
                ask::schema::aspect<resource::material::Runtime>(),
                ask::schema::aspect<resource::shadow::Asset>(),
                ask::schema::aspect<resource::shadow::Allocator>(),
                ask::schema::aspect<resource::shadow::Runtime>(),
                ask::schema::aspect<resource::geometry::Asset>(),
                ask::schema::aspect<resource::geometry::Loader>(),
                ask::schema::aspect<resource::geometry::Generator>(),
                ask::schema::aspect<resource::geometry::Runtime>(),
                ask::schema::aspect<resource::sprite::Pack>(),
                ask::schema::aspect<resource::sprite::LoaderKenney>(),
                ask::schema::aspect<resource::sprite::Runtime>(),
                ask::schema::aspect<controller::Camera3d>(),
                ask::schema::aspect<controller::Camera2d>(),
                ask::schema::aspect<scene::Root>(),
                ask::schema::aspect<scene::Node>(),
                ask::schema::aspect<scene::Node_group>(),
                ask::schema::aspect<scene::Camera>(),
                ask::schema::aspect<scene::Camera_group>(),
                ask::schema::aspect<scene::Light>(),
                ask::schema::aspect<scene::Light_group>(),
                ask::schema::aspect<scene::actor::Simple>(),
                ask::schema::aspect<scene::actor::Sprite>(),
                ask::schema::aspect<scene::Flat2d>(),
                ask::schema::aspect<scene::Grid>(),
            });
            return once;
        }

    } // namespace

    struct Engine::State : establish::Module::State {
        struct {
            maybe<system::Core::Id> core;
            maybe<system::Device::Id> device;
            std::vector<ViewContext> activeViews;
            maybe<resource::shadow::Asset::Id> default_shadow;
        } handles;

        struct {
            std::vector<GLFWwindow*> windows;
        } driver;
        Renderer renderer;

        explicit State(Schema schema)
            : establish::Module::State(std::move(schema))
        {}

        ~State() override {
            for (GLFWwindow* handle : driver.windows) {
                if (handle) {
                    glfwDestroyWindow(handle);
                }
            }
            if (not driver.windows.empty()) {
                glfwTerminate();
                driver.windows.clear();
            }
        }

        void setupDefaultShadow(Writing context, system::Core::Id core) {
            if (handles.default_shadow.exists())
                return;
            using resource::Assets;
            using resource::Unit;
            for (const auto entry : context->aspect<Unit>().items()) {
                if (entry.value.manager == core && entry.value.name == "main_shadow") {
                    handles.default_shadow = entry.id;
                    return;
                }
            }
            handles.default_shadow = with<Assets>::add_shadow_allocator(
                context,
                core,
                Unit::Quantum{.manager = core, .name = "main_shadow", .library = "rmmr"},
                resource::shadow::Allocator::Quantum{.size = index2{1024, 1024}});
        }

        void loadPastState(Writing) override {}
    };

    Engine::Engine() = default;
    Engine::~Engine() = default;

    Schema Engine::schema() {
        return engineDomain();
    }

    std::shared_ptr<establish::Module::State> Engine::install(Schema finalSchema) {
        state = std::make_shared<State>(std::move(finalSchema));
        return state;
    }

    auto Engine::setup(Writing context, item<system::Core> core, WindowParameters windowParams) -> system::Core::Id {
        base::message("rmmr: creating core...");
        const auto coreId = with<system::Interface>::create(context, std::move(core));
        state->handles.core = coreId;

        base::message("rmmr: creating device and window...");
        state->handles.device = with<system::Interface>::addDeviceAndWindow(
            context,
            coreId,
            std::move(windowParams.title),
            windowParams.requested_size,
            windowParams.presentation);
        return coreId;
    }

    void Engine::materialize(Writing context, system::Core::Id assets) {
        state->setupDefaultShadow(context, assets);
        with<resource::Runtimes>::materialize(context, *state->handles.device, assets);
    }

    auto Engine::core() const -> system::Core::Id {
        return *state->handles.core;
    }

    auto Engine::window() const -> system::Window::Id {
        return *state->handles.device;
    }

    void Engine::setActiveViews(std::vector<ViewContext> views) {
        state->handles.activeViews = std::move(views);
    }

    bool Engine::shouldClose(Reading context) const {
        return glfwWindowShouldClose(with<system::Device>::get(context, state->handles.device).handle);
    }

    void Engine::beginFrame(Writing context) {
        const auto& device = state->handles.device;

        with<system::Device>::poll_events(context);
        // Input first, then system clock — Clock reactions see fresh Window snapshots.
        with<system::Window>::onFrameAdvanced(context, device);

        {
            const auto core = with<system::Device>::get(context, device).core;
            const auto us = static_cast<int64>(glfwGetTime() * 1'000'000.0);
            with<system::Clock>::modify(context, core)->absolute = us;
        }

        {
            const auto& input = with<system::Window>::get(context, device).current;
            if (static_cast<std::size_t>(GLFW_KEY_ESCAPE) < input.keys.size()
                and input.keys[static_cast<std::size_t>(GLFW_KEY_ESCAPE)])
            {
                glfwSetWindowShouldClose(with<system::Device>::get(context, device).handle, true);
            }
        }

        if (not state->handles.activeViews.empty()) {
            with<system::ImGuiHost>::newFrame(context, device);
        }
    }

    void Engine::render(Writing context) {
        for (const auto& view : state->handles.activeViews) {
            with<system::Viewport>::syncExtent(context, view.viewport);
            with<system::Viewport>::activate(context, view.viewport);
            with<system::Viewport>::clear(context, view.viewport);

            /* natural perfomance test, keep this as comment please
            for (int xx = 0; xx < 100; ++xx)
                with<system::Viewport>::modify(context, view.viewport)->clear_color.r = 0;
            */

            state->renderer.render(Renderer::FrameContext{
                .world = context,
                .window = *state->handles.device,
                .view = view,
            });
        }
    }

    void Engine::endFrame(Writing context) {
        const auto& device = state->handles.device;
        if (not state->handles.activeViews.empty()) {
            with<system::ImGuiHost>::render(context, device);
        }
        with<system::Window>::present(context, device);
    }

    void Engine::shutdown(Writing context) noexcept {
        base::message("rmmr teardown: Engine shutdown begin");
        state->driver.windows.clear();
        for (const auto entry : context->aspect<system::Device>().items()) {
            if (entry.value.handle) {
                state->driver.windows.push_back(entry.value.handle);
            }
        }
        for (const auto& view : state->handles.activeViews) {
            auto viewport = maybe<system::Viewport::Id>{view.viewport};
            ask::temp_sugar::drop_reference<system::Viewport>(context, viewport);
        }
        state->handles.activeViews.clear();
        with<system::Interface>::shutdown(context);
        // Native GLFW teardown waits for ~State — after Writing collapses into Realm.
        base::message("rmmr teardown: Engine shutdown done");
    }
}
