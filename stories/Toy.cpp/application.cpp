#include "application.h"

#include <base/logging.h>
#include <base/maybe.h>
#include <rmmr/controller/camera.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/textures.q1.h>
#include <rmmr/scene/actor.q1.h>
#include <rmmr/scene/root.q1.h>

#include "projection/world.q1.h"
#include "assets/library.h"
#include "ui.h"

#include <stdexcept>
#include <utility>

namespace toy {
    using namespace fqsm::api;
    using namespace rmmr;

    struct Application::State : establish::Module::State {
        base::maybe<assets::Manager> assets;
        bool assets_ready = false;

        ui::State ui;
        establish::Realm world;

        explicit State(Schema schema);
        auto prepareAssets(filepath assets_root) -> assets::PrepareStatus;
        void spawnDemoScene(Writing);
        void loadPastState(Writing) override;
    };

    Application::State::State(Schema schema)
        : establish::Module::State(std::move(schema))
        , world(fullSchema)
    {}

    auto Application::State::prepareAssets(filepath assets_root) -> assets::PrepareStatus {
        const auto path = assets::Manager::statePath(assets_root);
        const auto status = assets->prepare(world, path);
        assets_ready = status != assets::PrepareStatus::Failed;
        return status;
    }

    void Application::State::spawnDemoScene(Writing context) {
        const auto& handles = assets->handles;
        const auto root = with<scene::Interface>::createScene(context);

        constexpr int grid_extent = 4;
        constexpr float cube_edge = 1.0f;
        constexpr float spacing = cube_edge * 1.5f;
        const float center_offset = (static_cast<float>(grid_extent) - 1.0f) * 0.5f;
        const float cluster_lift = center_offset * spacing + cube_edge * 0.5f;

        for (int z = 0; z < grid_extent; ++z) {
            for (int y = 0; y < grid_extent; ++y) {
                for (int x = 0; x < grid_extent; ++x) {
                    const int cell = x + y + z;
                    const bool alpha_cutout = (cell % 5 == 0);
                    const Pos pos{
                        (static_cast<float>(x) - center_offset) * spacing,
                        (static_cast<float>(y) - center_offset) * spacing + cluster_lift,
                        (static_cast<float>(z) - center_offset) * spacing,
                    };
                    with<scene::Interface>::createPrimitiveActor(context, root,
                        Locator{
                            .pos = pos,
                            .euler = HPB{
                                -22.5f + 45.0f * static_cast<float>(x),
                                -15.0f + 30.0f * static_cast<float>(y),
                                -12.0f + 24.0f * static_cast<float>(z),
                            },
                        },
                        item<scene::PrimitiveActor>{
                            .geometry = (cell % 7 == 0) ? *handles.primitive.bagel : *handles.primitive.kube,
                            .material = alpha_cutout ? *handles.material.litTexturedAlpha : handles.material.debugLitTextured[cell % 4],
                            .albedo = RGB{
                                0.3f + 0.6f * static_cast<float>(x) / static_cast<float>(grid_extent - 1),
                                0.3f + 0.6f * static_cast<float>(y) / static_cast<float>(grid_extent - 1),
                                0.3f + 0.6f * static_cast<float>(z) / static_cast<float>(grid_extent - 1),
                            },
                        });
                }
            }
        }

        with<scene::Interface>::createGrid(context, root,
            Locator{.pos = Pos{0.0f, 0.0f, 0.0f}, .euler = HPB{0.0f, 0.0f, 0.0f}},
            item<scene::Grid>{.geometry = *handles.primitive.grid, .material = *handles.material.grid, .opacity = 1.0f});

        ui.camera = with<scene::Interface>::createCamera(context, root,
            Locator{.pos = Pos{10.5f, 10.0f, 14.0f}, .euler = HPB{-18.0f, -36.0f, 0.0f}},
            item<scene::Camera>{.fov_y = 1.04719755f, .z_near = 0.1f, .z_far = 100.0f});
        with<controller::Camera>::create(context, ui.camera);
        with<scene::Interface>::createLight(context, root,
            Locator{.pos = Pos{9.5f, 19.0f, 7.5f}, .euler = HPB{0.0f, 0.0f, 0.0f}},
            item<scene::Light>{.color = RGB{1.0f, 0.94f, 0.86f}, .intensity = 7.0f, .range = 30.0f});

        ui.scene = root;
    }

    void Application::State::loadPastState(Writing) {
        _INCOMPLETE_;
    }


    //
    // Application
    //

    Application::Application(Settings settings)
        : settings(std::move(settings))
        , engine(add<rmmr::Engine>())
    {}

    Application::~Application() = default;

    Schema Application::schema() {
        static const Schema native = ask::schema::aspect<God>();
        Schema result = native;
        for (auto& child : submodules)
            result = ask::schema::merge({result, child->schema()});
        return result;
    }

    std::shared_ptr<establish::Module::State> Application::install(Schema schema) {
        state = std::make_shared<State>(std::move(schema));
        for (auto& child : submodules)
            child->install(state->fullSchema);
        return state;
    }

    void Application::initDefaultWorld() {
        const auto core = engine->setup(
            state->world,
            item<system::Core>{
                .assets_root = settings.assets_root,
                .version = system::Core::GLVer{
                    .major = settings.glVersion.major,
                    .minor = settings.glVersion.minor,
                },
            },
            Engine::WindowParameters{
                .title = settings.title,
                .requested_size = settings.window_size,
            });
        state->assets = assets::Manager{.core = core};

        const auto status = state->prepareAssets(settings.assets_root);
        if (status == assets::PrepareStatus::Failed) {
            base::message("toy: refusing to seed over a broken assets save; soft exit");
            return;
        }

        state->spawnDemoScene(state->world);
        engine->materialize(state->world, state->assets->core);
        engine->showScene(*state->ui.scene, *state->ui.camera);

        if (status == assets::PrepareStatus::Generated) {
            try {
                state->assets->save(state->world, assets::Manager::statePath(settings.assets_root));
            } catch (const std::exception& error) {
                base::message("toy: initial assets save failed: {}", error.what());
            }
        }

        if (not state->world.result().good())
            throw std::runtime_error("app: initDefaultWorld failed");
    }

    void Application::loadWorld(filepath) {
        state->loadPastState(state->world);
        if (not state->world.result().good())
            throw std::runtime_error("app: loadWorld failed");
    }

    int Application::run() {
        if (not state->assets_ready) {
            if (engine)
                engine->shutdown(state->world);
            return 1;
        }

        while (engine and not engine->shouldClose(state->world)) {
            engine->beginFrame(state->world);
            state->ui.draw(state->world);
            engine->render(state->world);
            engine->endFrame(state->world);
        }

        if (state->assets.exists()) {
            try {
                state->assets->save(state->world, assets::Manager::statePath(settings.assets_root));
            } catch (const std::exception& error) {
                base::message("toy: assets save failed: {}", error.what());
            }
        }

        if (engine)
            engine->shutdown(state->world);
        return 0;
    }

}
