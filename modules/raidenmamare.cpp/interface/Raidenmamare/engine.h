#pragma once

#include <string>
#include <memory>

#include <iQSM/schema.h>
#include <iQSM/repository/agents/subsystem.h>

// this include will be removed from "public" Engine interface after "Engine::StartupParameters" become facade type (just config file to read?)
#include <Raidenmamare/device.q1.h>

namespace rmmr {

    using namespace iqsm::q1;

    class Engine : public iqsm::agents::Subsystem {
    public:
        // temp: this will become separate type one day:
        using StartupParameters = Device::Materializer::Passport;

        explicit Engine(StartupParameters);
        ~Engine() noexcept;

        auto schema() const -> iqsm::Schema override;
        auto access() -> iqsm::agents::Subsystem::Update override;

        // Runs the OpenGL demo. Returns 0 on success, non-zero on failure.
        int run_render_demo();

    private:
        struct State;

        static iqsm::Schema schema_static();

        void prepareResources();
        void createScene();
        void createViewport(index2 size, index2 origin = index2{0, 0});
        void shutdown() noexcept;

        // only this is really needed...
        std::shared_ptr<State> state;
    };
}

