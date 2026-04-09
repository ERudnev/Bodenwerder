#include <iostream>

#include <iQSM/logger.h>
#include <Raidenmamare/core.h>


int main() {
    using namespace iqsm::logger;
    message("[{}] Test app is started...", to_string(now()));

    rmmr::Core renderer;
    return renderer.run_render_demo();
}