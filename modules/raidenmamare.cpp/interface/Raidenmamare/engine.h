#pragma once

#include <string>
#include <memory>

#include <fQSM/api/interface.h>
#include <fQSM/processing/orchestrators/subsystem.h>

// this include will be removed from "public" Engine interface after "Engine::StartupParameters" become facade type (just config file to read?)
#include <Raidenmamare/application.q1.h>

namespace rmmr {

    class Engine : public fqsm::processing::orchestrator::Subsystem {
    public:
        struct StartupParameters {
            string assets_root;
            decltype(Window::Quantum::title) title;
            decltype(Window::Quantum::size) size;
            integer context_major;
            integer context_minor;
        };

        explicit Engine(StartupParameters);
        ~Engine() noexcept;

        // fQSM interface to be changed soon
        fqsm::Schema interfaceSchema() const override { return interface; }
        // Runs the OpenGL demo. Returns 0 on success, non-zero on failure.
        int run_render_demo();

    private:
        void prepareResources();
        void createScene();
        void createViewport(index2 size, index2 origin = index2{0, 0});
        void shutdown() noexcept;

        struct State;
        const Schema interface;
        std::shared_ptr<State> state;
    };
}
