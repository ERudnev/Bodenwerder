#include <iostream>
#include <filesystem>

#include <base/logging.h>
#include <fQSM/api/interface.h>
#include <rmmr/engine.h>

#include "projection/world.q1.h"

using namespace base;
using namespace fqsm::api;

namespace {

    Schema generateSchema() {
        return ask::schema::merge({
            rmmr::Engine::moduleSchema(),
            ask::schema::aspect<toy::God>(),
        });
    }

} // namespace

int main() {
    message("[{}] Test app is started...", now());

    try {
        establish::Realm world{generateSchema()};
        rmmr::Engine renderer;

        const auto assets_root = std::filesystem::path(DAQL_ASSETS_DIR);
        // TODO: implement deseralize<Meta>()->MEta::Quantum;
        renderer.bootstrap(world, rmmr::Engine::StartupParameters{
            .assets_root = assets_root,
            .title = "Raidenmamare",
            .requested_size = rmmr::index2{.x = 1600, .y = 900},
            .context_major = 3,
            .context_minor = 3,
        });
        if (not world.result().good()) {
            throw std::runtime_error("Engine bootstrap failed");
        }

        while (not renderer.shouldClose(world)) {
            // return close decision here
            renderer.frame(world);
        }

        renderer.shutdown(world);
        return 0;
    } catch (const std::exception& e) {
        message("[{}] Engine error: {}", to_string(now()), e.what());
        return -1;
    } catch (...) {
        message("[{}] Engine error: unknown exception", to_string(now()));
        return -1;
    }
}
