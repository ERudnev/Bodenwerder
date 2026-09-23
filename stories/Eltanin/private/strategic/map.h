#pragma once

#include <vector>

#include <eltanin/totality/system.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::strategic {

    using namespace fqsm::api;

    struct Map {
        vector<totality::System::Id> systems;
    };

}
