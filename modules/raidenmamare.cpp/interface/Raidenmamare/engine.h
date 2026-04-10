#pragma once

#include <string>

#include <iQSM/api/_gateway.h>

#include <iQSM/schema.h>

#include <Raidenmamare/core.q1.h>

namespace rmmr {
    class Engine {
    public:
        explicit Engine(std::string assetsRoot);

        // Runs the OpenGL demo. Returns 0 on success, non-zero on failure.
        int run_render_demo();

    private:
        const std::string assetsRoot;
        iqsm::repo::Branch state;
        iqsm::dsl_gateway::resources::Manager resourceManager;
        const Core::Id core;

        static iqsm::Schema resourceAspects();

        void shutdown();
    };
}

