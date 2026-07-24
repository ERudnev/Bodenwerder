#include "application.h"

#include <base/logging.h>
#include <base/maybe.h>
#include <rmmr/system/core.q1.h>

#include "projection/world.q1.h"
#include "assets/library.h"
#include "demo.h"
#include "demos/spriteTest.h"
#include "demos/kubeOfKubes.h"
#include "ui.h"

#include <memory>
#include <stdexcept>
#include <utility>

namespace toy {
    using namespace fqsm::api;
    using namespace rmmr;

    struct Application::State : establish::Module::State {
        base::maybe<assets::Manager> assets;
        bool assets_ready = false;
        std::unique_ptr<Demo> demo;

        ui::State ui;
        establish::Realm world;

        explicit State(Schema schema);
        auto prepareAssets(filepath assets_root) -> assets::PrepareStatus;
        void loadPastState(Writing) override;
    };

    Application::State::State(Schema schema)
        : establish::Module::State(std::move(schema))
        // , demo(std::make_unique<demos::SpriteTest>())
        , demo(std::make_unique<demos::KubeOfKubes>())
        , world(fullSchema)
    {}

    auto Application::State::prepareAssets(filepath assets_root) -> assets::PrepareStatus {
        const auto path = assets::Manager::statePath(assets_root);
        const auto status = assets->prepare(world, path);
        assets_ready = status != assets::PrepareStatus::Failed;
        return status;
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

        state->demo->seedAssets(state->world, core, state->assets->handles);
        const auto demo_handles = state->demo->setup(state->world, state->assets->handles);
        state->ui.scene = demo_handles.scene;
        state->ui.camera = demo_handles.camera;
        engine->materialize(state->world, state->assets->core);
        engine->showScene(demo_handles.scene, demo_handles.camera);

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
