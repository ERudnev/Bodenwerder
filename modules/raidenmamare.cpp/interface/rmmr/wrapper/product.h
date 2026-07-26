#pragma once

#include <vector>

#include <fQSM/api/interface.h>
#include <rmmr/engine.h>
#include <rmmr/system/core.q1.h>
#include <rmmr/system/viewport.q1.h>

namespace rmmr::wrapper {

    using namespace fqsm::api;

    class Product {
    public:
        using View = Engine::ViewContext;

        std::vector<View> views;

        virtual ~Product() = default;

        virtual Schema schema() const = 0;

        virtual void addAssets(Writing, system::Core::Id) = 0;
        virtual void setup(Writing, system::Core::Id, system::Viewport::Id) = 0;

        virtual void contributeViewMenu() = 0;
        virtual void drawUi(Writing) = 0;
    };

}
