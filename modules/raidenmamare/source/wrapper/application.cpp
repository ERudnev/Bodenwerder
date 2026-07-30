#include <rmmr/wrapper/application.h>

#include <base/logging.h>
#include <base/maybe.h>
#include <rmmr/system/core.q1.h>
#include <rmmr/wrapper/library.h>
#include <rmmr/wrapper/ui.h>

#include <memory>
#include <stdexcept>
#include <utility>

namespace rmmr::wrapper {
    using namespace fqsm::api;
    using namespace rmmr;

    struct Application::State : establish::Module::State {
        base::maybe<assets::Manager> assets;
        bool assets_ready = false;

        ui::State ui;
        establish::RealmSafe world;

        explicit State(Schema schema);
        auto prepareAssets(filepath assets_root) -> assets::PrepareStatus;
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

    void Application::setProduct(std::unique_ptr<Product> next) {
        product = std::move(next);
    }

    Schema Application::schema() {
        if (not product)
            throw std::runtime_error("app: setProduct before schema()");

        Schema result = product->schema();
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
        if (not product)
            throw std::runtime_error("app: setProduct before initDefaultWorld()");

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
                .presentation = settings.presentation,
            });
        state->world.finish_patch();
        state->assets = assets::Manager{.core = core};

        const auto status = state->prepareAssets(settings.assets_root);
        state->world.finish_patch();
        if (status == assets::PrepareStatus::Failed) {
            base::message("app: refusing to seed over a broken assets save; soft exit");
            return;
        }

        product->bindShared(state->assets->handles);
        product->addAssets(state->world, core);
        state->world.finish_patch();
        product->setup(state->world, core, engine->window());
        state->world.finish_patch();
        engine->materialize(state->world, state->assets->core);
        state->world.finish_patch();
        engine->setActiveViews(product->views);

        if (status == assets::PrepareStatus::Generated) {
            try {
                state->assets->save(state->world, assets::Manager::statePath(settings.assets_root));
            } catch (const std::exception& error) {
                base::message("app: initial assets save failed: {}", error.what());
            }
            state->world.finish_patch();
        }

        if (not state->world.result().good())
            throw std::runtime_error("app: initDefaultWorld failed");
    }

    void Application::loadWorld(filepath) {
        state->loadPastState(state->world);
        state->world.finish_patch();
        if (not state->world.result().good())
            throw std::runtime_error("app: loadWorld failed");
    }

    int Application::run() {
        if (not product)
            throw std::runtime_error("app: setProduct before run()");

        if (not state->assets_ready) {
            if (engine) {
                engine->shutdown(state->world);
                state->world.finish_patch();
            }
            return 1;
        }

        while (engine and not engine->shouldClose(state->world)) {
            engine->beginFrame(state->world);
            state->world.finish_patch();
            state->ui.draw(state->world, *product);
            state->world.finish_patch();
            engine->render(state->world);
            state->world.finish_patch();
            engine->endFrame(state->world);
            state->world.finish_patch();
        }

        if (state->assets.exists()) {
            try {
                state->assets->save(state->world, assets::Manager::statePath(settings.assets_root));
            } catch (const std::exception& error) {
                base::message("app: assets save failed: {}", error.what());
            }
            state->world.finish_patch();
        }

        if (engine) {
            engine->shutdown(state->world);
            state->world.finish_patch();
        }
        return 0;
    }

}
