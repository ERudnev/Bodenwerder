#pragma once

#include <memory>

#include <fQSM/api/interface.h>

#include <rmmr/system/window.q1.h>

namespace rmmr {

    using namespace fqsm::api;

    class Engine  {
    public:
        struct StartupParameters {
            filepath assets_root;
            decltype(system::Window::Quantum::title) title;
            index2 requested_size;
            integer context_major;
            integer context_minor;
        };

        static Schema moduleSchema();

        Engine();
        ~Engine();

        void bootstrap(Writing, StartupParameters);
        bool shouldClose(Reading) const;
        void frame(Writing);
        void shutdown(Writing) noexcept;

    private:
        void prepareResources(Writing);
        void prepareRenderTargets();
        void createScene(Writing);
        void createViewport(Writing, index2 size, index2 origin = index2{0, 0});

        struct State;
        std::shared_ptr<State> state;
    };
}
