#include <rmmr/engine.h>

#include <base/logging.h>
#include <base/maybe.h>
#include <stdexcept>
#include <vector>

#include <rmmr/controller/camera2d.q1.h>
#include <rmmr/controller/camera3d.q1.h>
#include <rmmr/controller/cameraOrbit.q1.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/meshpack.q1.h>
#include <rmmr/resources/overlays.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/resources/shadows.q1.h>
#include <rmmr/scene/actors/family.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/actors/patchGrid.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/system/core.q1.h>
#include <rmmr/system/imgui.q1.h>
#include <rmmr/system/interface.q1.h>
#include <rmmr/system/viewport.q1.h>
#include <rmmr/system/viewInput.q1.h>

#include "renderer/renderer.h"

#include <GLFW/glfw3.h>

namespace rmmr {
    using namespace fqsm::api;

    namespace {

        Schema engineDomain() {
            static const Schema once = ask::schema::merge({
                system::doctrine::core(),
                system::doctrine::imgui(),
                system::doctrine::window(),
                system::doctrine::viewInput(),
                system::doctrine::viewport(),
                resource::doctrine::manager(),
                resource::doctrine::runtimes(),
                resource::texture::doctrine::textures(),
                resource::texpack::doctrine::texpack(),
                resource::texture3array::doctrine::texture3array(),
                resource::shader::doctrine::shaders(),
                resource::material::doctrine::materials(),
                resource::overlay::doctrine::overlays(),
                resource::shadow::doctrine::shadows(),
                resource::geometry::doctrine::geometry(),
                resource::sprite::doctrine::sprites(),
                resource::meshpack::doctrine::meshpack(),
                controller::doctrine::camera3d(),
                controller::doctrine::camera2d(),
                controller::doctrine::cameraOrbit(),
                scene::doctrine::root(),
                scene::doctrine::node(),
                scene::doctrine::camera(),
                scene::doctrine::light(),
                scene::actor::doctrine::mesh(),
                scene::actor::doctrine::sprite(),
                scene::actor::doctrine::family(),
                scene::actor::doctrine::patchGrid(),
                scene::doctrine::gizmos(),
            });
            return once;
        }

    } // namespace

    struct Engine::State : establish::Module::State {
        struct {
            maybe<system::Device::Id> device;
            std::vector<ViewContext> activeViews;
            maybe<resource::shadow::Asset::Id> default_shadow;
            maybe<resource::overlay::Asset::Id> activeOverlay;
            std::vector<renderer::Integer32> overlaySelection;
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

        void setupDefaultShadow(Writing context) {
            if (handles.default_shadow.has_value())
                return;
            using resource::Assets;
            using resource::Unit;
            for (const auto entry : context->aspect<Unit>().items()) {
                if (entry.value.name.own == "main_shadow") {
                    handles.default_shadow = entry.id;
                    return;
                }
            }
            handles.default_shadow = with<Assets>::add_shadow_allocator(
                context,
                Unit::Name::from("rmmr", "main_shadow"),
                resource::shadow::Allocator::Quantum{.size = index2{2048, 2048}});
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

    void Engine::createCore(Writing context, item<system::Core> core) {
        base::message("rmmr: phase createCore");
        with<system::Interface>::create(context, std::move(core));
    }

    void Engine::prepareAssets(Writing context) {
        base::message("rmmr: phase prepareAssets");
        with<resource::Manager>::load(context);
    }

    void Engine::createWindow(Writing context, WindowParameters windowParams) {
        base::message("rmmr: phase createWindow");
        state->handles.device = with<system::Interface>::addDeviceAndWindow(
            context,
            std::move(windowParams.title),
            windowParams.requested_size,
            windowParams.presentation);
    }

    void Engine::materialize(Writing context) {
        base::message("rmmr: phase materialize");
        state->setupDefaultShadow(context);
        with<resource::Runtimes>::materialize(context, *state->handles.device);
    }

    auto Engine::window() const -> system::Window::Id {
        return *state->handles.device;
    }

    void Engine::setActiveViews(std::vector<ViewContext> views) {
        state->handles.activeViews = std::move(views);
    }

    void Engine::setActiveOverlay(base::maybe<resource::overlay::Asset::Id> overlay) {
        state->handles.activeOverlay = std::move(overlay);
    }

    void Engine::setOverlaySelection(std::span<const renderer::Integer32> selection) {
        state->handles.overlaySelection.assign(selection.begin(), selection.end());
    }

    bool Engine::shouldClose(Reading context) const {
        return glfwWindowShouldClose(with<system::Device>::get(context, *state->handles.device).handle);
    }

    auto Engine::monotonicUs() const -> int64 {
        return static_cast<int64>(glfwGetTime() * 1'000'000.0);
    }

    void Engine::beginFrame(Writing context) {
        const auto device = *state->handles.device;

        with<system::Device>::poll_events(context);
        // Input snapshot, ImGui NewFrame, sanitize Window, then copy that into engaged view mails.
        with<system::Window>::onFrameAdvanced(context, device);

        if (not state->handles.activeViews.empty()) {
            with<system::ImGuiHost>::newFrame(context, device);
            with<system::Window>::applyUiCapture(context, device);
        }
        with<system::ViewInput>::refresh(context, device);

        {
            const auto clock = with<system::Clock>::singleton(context);
            const auto us = static_cast<int64>(glfwGetTime() * 1'000'000.0);
            with<system::Clock>::modify(context, clock)->absolute = us;
        }

        {
            const auto& input = with<system::Window>::get(context, device).current;
            if (static_cast<std::size_t>(GLFW_KEY_ESCAPE) < input.keys.size()
                and input.keys[static_cast<std::size_t>(GLFW_KEY_ESCAPE)])
            {
                glfwSetWindowShouldClose(with<system::Device>::get(context, device).handle, true);
            }
        }
    }

    void Engine::render(Writing context) {
        scene::actor::Identified::Actions::applySelection(context, state->handles.overlaySelection);
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
                .overlay = state->handles.activeOverlay,
                .selection = state->handles.overlaySelection,
            });
        }
    }

    void Engine::endFrame(Writing context) {
        const auto device = *state->handles.device;
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
