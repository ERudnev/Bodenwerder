#pragma once

#include <string>
#include <memory>

#include <iQSM/api/_gateway.h>
#include <iQSM/schema.h>

// this include will be removed from "public" Engine interface after "Engine::StartupParameters" become facade type (just config file to read?)
#include <Raidenmamare/core.q1.h>


namespace rmmr::internal {
    // local forward to Engine internal state:
    struct EngineState;
}

namespace rmmr {

    class Engine {
    public:
        // temp: this will become separate type one day:
        using StartupParameters = Core::Materializer::Passport;

        explicit Engine(StartupParameters);
        ~Engine() noexcept;

        // Runs the OpenGL demo. Returns 0 on success, non-zero on failure.
        int run_render_demo();

    private:
        using State = internal::EngineState;

        static iqsm::Schema resourceAspects();
        void prepareResources();
        void shutdown() noexcept;

        // only this is really needed...
        std::shared_ptr<State> state;
    };
}

