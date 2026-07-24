#pragma once

#include <fQSM/api/interface.h>

#include "demo.h"

namespace toy::ui {

    using namespace fqsm::api;

    // Application-level UI: View menu shell + Stats.
    // Demo panels contribute via Demo::contributeViewMenu / drawUi.
    struct State {
        bool stats = true;

        void draw(Writing, Demo&, const Demo::Handles&);

    private:
        void drawMainMenuBar(Demo&);
        void drawStatsWindow(Writing);
    };

}
