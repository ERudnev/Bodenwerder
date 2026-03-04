#include <iostream>

#include <Atomic/varph.q1.h>
#include <iQSM/logger.h>

#include "model/model.h"
#include <Raidenmamare/core.h>

Toy::Model model(""); // empty string: non-persistent model

int main() {
    using namespace iqsm::logger;
    message("[{}] Test app is started...", to_string(now()));
    model.create();

    rmmr::Core renderer;
    return renderer.run_render_demo();
}