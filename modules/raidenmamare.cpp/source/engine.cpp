#include <Raidenmamare/engine.h>
#include <Raidenmamare/viewport.q1.h>

#include <base/logging.h>

namespace {
    using namespace fqsm::api;
    using namespace rmmr;

    Schema generateInternalEngineSchema_static() {
        return ask::schema::merge({
            ask::schema::aspect<Window>(),
            ask::schema::aspect<Device>(),
            ask::schema::aspect<Viewport>(),
        });
    }

    Schema generateInterfaceEngineSchema_static() {
        return ask::schema::merge({
            ask::schema::aspect<Window>(),
        });
    }
}

namespace rmmr {
    using namespace fqsm::api;

    struct Engine::State {
        establish::Realm main;
    };

    Engine::Engine(StartupParameters)
        : interface(generateInterfaceEngineSchema_static())
        , state(std::make_shared<State>(State{
            .main = establish::Realm{generateInternalEngineSchema_static()},
        }))
    {
    }

    Engine::~Engine() noexcept {
        shutdown();
    }

    int Engine::run_render_demo() {
        _INCOMPLETE_;
    }

    void Engine::prepareResources() {
        _INCOMPLETE_;
    }

    void Engine::createScene() {
        _INCOMPLETE_;
    }

    void Engine::createViewport(index2, index2) {
        _INCOMPLETE_;
    }

    void Engine::shutdown() noexcept {
    }
}
