#include <iostream>
#include <filesystem>

#include <iQSM/logger.h>
#include <Raidenmamare/engine.h>

/*
struct State {
    std::unique_ptr<rmmr::Engine> engine;

};


State createState() {
    State state;

    state = std::make_shared<
}
*/

int main() {
    using namespace iqsm::logger;
    message("[{}] Test app is started...", to_string(now()));

    const auto assets_root = std::filesystem::path(DAQL_ASSETS_DIR).string();
    const auto engine_startup_parameters = rmmr::Engine::StartupParameters{
        .assets_root = assets_root,
        .title = "Raidenmamare",
        .size = rmmr::index2{.x = 3000, .y = 1200},
        .context_major = 3,
        .context_minor = 3,
    };

    rmmr::Engine renderer(engine_startup_parameters);
    return renderer.run_render_demo();
}