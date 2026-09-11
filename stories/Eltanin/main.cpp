#include <algorithm>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

#include <base/logging.h>
#include <rmmr/api/_interface.h>

#include "story.h"

using namespace base;
using namespace fqsm::api;

int main(int argc, char** argv) {
    message("[{}] Test app is started...", now());

    try {
        base::maybe<std::filesystem::path> capture_path;
        integer capture_after = 180;
        for (int index = 1; index < argc; ++index) {
            const std::string_view argument{argv[index]};
            if (argument == "--capture" and index + 1 < argc) {
                capture_path = std::filesystem::path{argv[++index]};
            } else if (argument == "--capture-after" and index + 1 < argc) {
                capture_after = std::max<integer>(std::stoi(argv[++index]), 1);
            }
        }

        base::maybe<rmmr::api::Application::FrameCapture> capture;
        if (capture_path) {
            capture = rmmr::api::Application::FrameCapture{
                .destination = *capture_path,
                .after_frames = capture_after,
                .close_after = true,
            };
        }

        auto application = std::make_shared<rmmr::api::Application>(rmmr::api::Application::Settings{
            .assets_root = std::filesystem::path(DAQL_ASSETS_DIR),
            .title = "Eltanin",
            .window_size = {.x = 1600, .y = 900},
            .presentation = capture ? rmmr::system::Window::Presentation::windowed : rmmr::system::Window::Presentation::maximized,
            .glVersion = {.major = 4, .minor = 6},
            .capture = capture,
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
