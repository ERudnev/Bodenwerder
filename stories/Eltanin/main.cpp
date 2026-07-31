#include <filesystem>
#include <memory>
#include <stdexcept>

#include <base/logging.h>
#include <rmmr/api/_interface.h>

#include "story.h"

using namespace base;
using namespace fqsm::api;

int main() {
    message("[{}] Test app is started...", now());

    try {
        auto application = std::make_shared<rmmr::api::Application>(rmmr::api::Application::Settings{
            .assets_root = std::filesystem::path(DAQL_ASSETS_DIR),
            .title = "Eltanin",
            .window_size = {.x = 1600, .y = 900},
            .presentation = rmmr::system::Window::Presentation::maximized,
            .glVersion = {.major = 3, .minor = 3},
        });

        application->setProduct(std::make_unique<eltanin::Game>());

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
