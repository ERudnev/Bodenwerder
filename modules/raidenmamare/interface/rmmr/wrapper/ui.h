#pragma once

#include <fQSM/api/interface.h>

#include <rmmr/wrapper/product.h>

namespace rmmr::wrapper::ui {

    using namespace fqsm::api;

    struct State {
        bool stats = true;

        void draw(Writing, Product&);

    private:
        void drawMainMenuBar(Product&);
        void drawStatsWindow(Writing);
    };

}
