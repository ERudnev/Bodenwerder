#include <iostream>
#include <filesystem>

#include <iQSM/logger.h>
#include <Raidenmamare/engine.h>

int main() {
    using namespace iqsm::logger;
    message("[{}] Test app is started...", to_string(now()));

    const auto assets_root = (std::filesystem::path(DAQL_ASSETS_DIR) / "raidenmamare").string();
    rmmr::Engine renderer(assets_root);
    return renderer.run_render_demo();
}