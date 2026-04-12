#pragma once

#include <Raidenmamare/device.q1.h>
#include <Raidenmamare/materials/core.q1.h>

#include <iQSM/api/_gateway.h>

namespace rmmr::material {

    using namespace iqsm::dsl_gateway;

    struct MaterialGenerator final {
        static auto ambient(Writing, rmmr::Device::Id, resources::Manager) -> Core::Id;
        static auto lit(Writing, rmmr::Device::Id, resources::Manager) -> Core::Id;
        static auto grid(Writing, rmmr::Device::Id, resources::Manager) -> Core::Id;
    };

}
