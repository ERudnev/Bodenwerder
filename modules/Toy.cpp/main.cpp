#include <iostream>

#include <Atomic/elementary.q1.h>
#include <iQSM/logger.h>

#include "render_demo.h"

static void test_atomic_golden() {
    using namespace iqsm::logger;
    fatal("atomic model is empty, blocking application run..."); // will add "now()" to the message in "fatal"
}

int main() {
    using namespace iqsm::logger;
    message("[{}] Test app is started...", to_string(now()));
    test_atomic_golden();

    return Toy::run_render_demo();
}