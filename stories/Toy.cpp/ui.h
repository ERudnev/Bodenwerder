#pragma once

#include <fQSM/api/interface.h>

#include "product.h"

namespace toy::ui {

    using namespace fqsm::api;

    struct State {
        bool stats = true;

        void draw(Writing, toy::Product&);

    private:
        void drawMainMenuBar(toy::Product&);
        void drawStatsWindow(Writing);
    };

}
