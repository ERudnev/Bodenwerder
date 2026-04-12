#pragma once

#include <Raidenmamare/device.q1.h>
#include <Raidenmamare/materials/core.q1.h>

#include <iQSM/api/_gateway.h>

namespace rmmr::material {

    using namespace iqsm::dsl_gateway;

    struct MaterialGenerator final {
        // Declares shader program + material::Core (sandbox defaults). Staged delta is pushed into the commit.
        static auto ambient(Writing, rmmr::Device::Id, resources::Manager) -> Core::Id;
        static auto lit(Writing, rmmr::Device::Id, resources::Manager) -> Core::Id;
    };

}
