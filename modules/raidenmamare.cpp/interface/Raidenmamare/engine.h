#pragma once

#include <string>

#include <iQSM/api/_gateway.h>

#include <iQSM/schema.h>

#include <Raidenmamare/core.q1.h>

namespace rmmr {
    class Engine {
    public:
        struct Passport {
            Core::Materializer::Passport core;
        };

        explicit Engine(Passport passport);
        ~Engine() noexcept;

        // Runs the OpenGL demo. Returns 0 on success, non-zero on failure.
        int run_render_demo();

    private:
        const Passport passport;
        iqsm::repo::Branch state;
        iqsm::dsl_gateway::resources::Manager resourceManager;
        const Core::Id core;

        static iqsm::Schema resourceAspects();

        void shutdown() noexcept;
    };
}

