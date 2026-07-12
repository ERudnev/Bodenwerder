#pragma once

#include <rmmr/assets/material.q1.h>
#include <rmmr/resources/material.q1.h>
#include <rmmr/system/core.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::material {

    using namespace fqsm::api;

    struct MaterialGenerator final {
        static auto ambient(fqsm::Writing, system::Device::Id) -> resource::Material::Id;
        static auto lit(fqsm::Writing, system::Device::Id) -> resource::Material::Id;
        static auto grid(fqsm::Writing, system::Device::Id) -> resource::Material::Id;
        static auto shadowDepth(fqsm::Writing, system::Device::Id) -> resource::Material::Id;
    };

}
