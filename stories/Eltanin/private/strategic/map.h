#pragma once

#include <vector>

#include <eltanin/fundamental/system.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::strategic {

    using namespace fqsm::api;

    struct Map {
        vector<fundamental::System::Id> systems;
    };

}
