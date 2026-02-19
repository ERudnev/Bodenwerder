#include <iostream>

#include <Atomic/varph.q1.h>
#include <iQSM/logger.h>

#include "model/model.h"
#include "renderer/renderer.h"

Toy::Model model(""); // empty string: non-persistent model

int main() {
    using namespace iqsm::logger;
    message("[{}] Test app is started...", to_string(now()));
    model.create();

    return Toy::run_render_demo();
}