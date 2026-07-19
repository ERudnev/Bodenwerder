#include <filesystem>
#include <stdexcept>

#include <base/logging.h>

#include "application.h"

using namespace base;
using namespace fqsm::api;

int main() {
    message("[{}] Test app is started...", now());

    try {
        auto application = std::make_shared<toy::Application>(toy::Application::Settings{
            .assets_root = std::filesystem::path(DAQL_ASSETS_DIR),
            .title = "Raidenmamare",
            .window_size = {.x = 1600, .y = 900},
            .glVersion = {.major = 3, .minor = 3},
        });

        const auto schema = application->schema();
        application->install(schema);
        application->initDefaultWorld();
        return application->run();
    } catch (const std::exception& e) {
        message("[{}] Engine error: {}", to_string(now()), e.what());
        return -1;
    } catch (...) {
        message("[{}] Engine error: unknown exception", to_string(now()));
        return -1;
    }
}
