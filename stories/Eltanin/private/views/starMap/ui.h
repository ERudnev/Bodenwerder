#pragma once

#include <base/maybe.h>

#include <fQSM/api/interface.h>

namespace eltanin::views::starmap {

    using namespace fqsm::api;

    struct Menu {
        struct Blueprints {};
        base::maybe<Blueprints> blueprints;
    };

}
