#pragma once

#include <Raidenmamare/device.q1.h>
#include <Raidenmamare/materials/core.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::material {

    using namespace fqsm::api;

    struct MaterialGenerator final {
        static auto ambient(fqsm::Writing, Window::Id) -> Core::Id;
        static auto lit(fqsm::Writing, Window::Id) -> Core::Id;
        static auto grid(fqsm::Writing, Window::Id) -> Core::Id;
    };

}
