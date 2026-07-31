#pragma once

#include <vector>

#include <fQSM/api/interface.h>
#include <rmmr/engine.h>
#include <rmmr/system/core.q1.h>
#include <rmmr/system/window.q1.h>
#include <rmmr/wrapper/library.h>

namespace rmmr::wrapper {

    using namespace fqsm::api;

    class Product {
    public:
        using View = Engine::ViewContext;

        std::vector<View> views;

        virtual ~Product() = default;

        void bindShared(const assets::Handles& handles) { shared = &handles; }

        virtual Schema schema() const = 0;

        virtual void addAssets(Writing) = 0;
        virtual void setup(establish::Realm&, system::Window::Id) = 0;
        // App frame pulse: wall dt in microseconds (glfw). Product owns sim cadence.
        virtual void onFrame(establish::Realm&, int64 dt_us) = 0;

        virtual void contributeViewMenu() = 0;
        virtual void drawUi(Writing) = 0;

    protected:
        const assets::Handles* shared = nullptr;
    };

}
