#include <iostream>
#include <filesystem>

#include <iQSM/logger.h>
#include <Raidenmamare/engine.h>

int main() {
    using namespace iqsm::logger;
    message("[{}] Test app is started...", to_string(now()));

    const auto assets_root = (std::filesystem::path(DAQL_ASSETS_DIR) / "raidenmamare").string();
    const auto engine_startup_parameters = rmmr::Engine::StartupParameters{
        .assets_root = assets_root,
        .title = "Raidenmamare",
        .size = rmmr::index2{.x = 800, .y = 600},
        .context_major = 3,
        .context_minor = 3,
    };

    rmmr::Engine renderer(engine_startup_parameters);
    return renderer.run_render_demo();
}