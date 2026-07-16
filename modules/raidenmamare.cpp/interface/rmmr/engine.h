#pragma once

#include <memory>

#include <fQSM/api/interface.h>
#include <fQSM/processing/orchestrators/subsystem.h>

#include <rmmr/system/window.q1.h>

namespace rmmr {

    using namespace fqsm::api;

    class Engine : public fqsm::processing::orchestrator::Subsystem {
    public:
        struct StartupParameters {
            filepath assets_root;
            decltype(system::Window::Quantum::title) title;
            index2 requested_size;
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
        void prepareRenderTargets();
        void createScene();
        void createViewport(index2 size, index2 origin = index2{0, 0});
        void shutdown() noexcept;

        struct State;
        const Schema interface;
        std::shared_ptr<State> state;
    };
}
