#pragma once

#include <string>
#include <memory>

#include <iQSM/schema.h>

// this include will be removed from "public" Engine interface after "Engine::StartupParameters" become facade type (just config file to read?)
#include <Raidenmamare/device.q1.h>


namespace rmmr::internal {
    // local forward to Engine internal state:
    struct EngineState;
}

namespace rmmr {

    using namespace iqsm::q1;

    class Engine {
    public:
        // temp: this will become separate type one day:
        using StartupParameters = Device::Materializer::Passport;

        explicit Engine(StartupParameters);
        ~Engine() noexcept;

        // Runs the OpenGL demo. Returns 0 on success, non-zero on failure.
        int run_render_demo();

    private:
        using State = internal::EngineState;

        static iqsm::Schema resourceAspects();
        void prepareResources();
        void createScene();
        void createViewport(index2 size, index2 origin = index2{0, 0});
        void shutdown() noexcept;

        // only this is really needed...
        std::shared_ptr<State> state;
    };
}

