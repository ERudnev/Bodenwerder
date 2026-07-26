#pragma once

#include <memory>
#include <vector>

#include <fQSM/api/interface.h>

#include <rmmr/scene/camera.q1.h>
#include <rmmr/scene/root.q1.h>
#include <rmmr/system/core.q1.h>
#include <rmmr/system/window.q1.h>
#include <rmmr/system/viewport.q1.h>

namespace rmmr {

    using namespace fqsm::api;

    class Engine : public establish::Module {
    public:
        struct WindowParameters {
            string title;
            index2 requested_size;
        };

        struct ViewContext {
            system::Viewport::Id viewport;
            scene::Root::Id scene;
            scene::Camera::Id camera;
        };

        Engine();
        ~Engine() override;

        Schema schema() override;
        std::shared_ptr<establish::Module::State> install(Schema finalSchema) override;

        // Creates system::Core on the caller's Writing, then device/window.
        auto setup(Writing, item<system::Core>, WindowParameters) -> system::Core::Id;
        void materialize(Writing, system::Core::Id assets);
        auto core() const -> system::Core::Id;
        auto viewport() const -> system::Viewport::Id;
        void setActiveViews(std::vector<ViewContext>);

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
