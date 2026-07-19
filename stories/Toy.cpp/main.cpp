#include <filesystem>
#include <stdexcept>

#include <base/logging.h>

#include "application.h"

using namespace base;
using namespace fqsm::api;

int main() {
    message("[{}] Test app is started...", now());

    try {
        toy::Application application{{
            .assets_root = std::filesystem::path(DAQL_ASSETS_DIR),
            .title = "Raidenmamare",
            .window_size = {.x = 1600, .y = 900},
            .glVersion = {.major = 3, .minor = 3},
        }};

        application.addEngine();
        const auto schema = application.composedDomain();
        application.install(schema);
        establish::Realm world{schema};
        application.initDefaultWorld(world);
        return application.run(world);
    } catch (const std::exception& e) {
        message("[{}] Engine error: {}", to_string(now()), e.what());
        return -1;
    } catch (...) {
        message("[{}] Engine error: unknown exception", to_string(now()));
        return -1;
    }
}
