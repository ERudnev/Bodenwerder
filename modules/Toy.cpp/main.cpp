#include <iostream>
#include <filesystem>

#include <base/logging.h>
#include <fQSM/api/interface.h>
#include <rmmr/engine.h>


struct State {
    std::shared_ptr<rmmr::Engine> renderer;
};


int main() {
    using namespace base;
    using namespace fqsm::api;
    message("[{}] Test app is started...", now());

    try {
        State state;

        const auto assets_root = std::filesystem::path(DAQL_ASSETS_DIR).string();
        // TODO: implement deseralize<Meta>()->MEta::Quantum;
        const auto engine_startup_parameters = rmmr::Engine::StartupParameters{
            .assets_root = assets_root,
            .title = "Raidenmamare",
            .requested_size = rmmr::index2{.x = 3000, .y = 1200},
            .context_major = 3,
            .context_minor = 3,
        };

        state.renderer = std::make_shared<rmmr::Engine>(engine_startup_parameters);
        // TODO: ask renderer interface different way?
        //const auto schema = engineObj->schema();

        return state.renderer->run_render_demo();
    } catch (const std::exception& e) {
        message("[{}] Engine error: {}", to_string(now()), e.what());
        return -1;
    } catch (...) {
        message("[{}] Engine error: unknown exception", to_string(now()));
        return -1;
    }
}