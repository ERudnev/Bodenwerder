#pragma once

#include <memory>

#include <fQSM/api/interface.h>

#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/system/core.q1.h>
#include <rmmr/system/window.q1.h>

namespace rmmr {

    using namespace fqsm::api;

    class Engine : public establish::Module {
    public:
        struct WindowParameters {
            decltype(system::Window::Quantum::title) title;
            index2 requested_size;
        };

        Engine();
        ~Engine() override;

        Schema schema() override;
        std::shared_ptr<establish::Module::State> installState(Schema finalSchema) override;

        void setup(Writing, establish::Module::RootId&, WindowParameters);
        void materialize(Writing, system::Core::Id assets);
        void showScene(scene::Root::Id, scene::Camera::Id);

        bool shouldClose(Reading) const;
        void beginFrame(Writing);
        void render(Writing);
        void endFrame(Writing);
        void shutdown(Writing) noexcept;

    private:
        void createViewport(Writing, index2 size, index2 origin = index2{0, 0});

        struct State;
        std::shared_ptr<State> state;
    };

}
