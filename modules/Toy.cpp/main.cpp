#include <iostream>

#include <iQSM/logger.h>
#include <Raidenmamare/engine.h>


int main() {
    using namespace iqsm::logger;
    message("[{}] Test app is started...", to_string(now()));

    rmmr::Engine renderer("assets/raidenmamare");
    return renderer.run_render_demo();
}