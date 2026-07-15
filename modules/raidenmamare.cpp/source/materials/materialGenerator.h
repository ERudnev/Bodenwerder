#pragma once

#include <rmmr/assets/material.q1.h>
#include <rmmr/assets/texture.q1.h>
#include <rmmr/resources_old/material.q1.h>
#include <rmmr/system/core.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::material {

    using namespace fqsm::api;

    struct MaterialGenerator final {
        static auto ambient(fqsm::Writing, system::Device::Id) -> resource_old::Material::Id;
        static auto lit(fqsm::Writing, system::Device::Id) -> resource_old::Material::Id;
        static auto litTextured(fqsm::Writing, system::Device::Id, asset::Texture::Id) -> resource_old::Material::Id;
        static auto grid(fqsm::Writing, system::Device::Id) -> resource_old::Material::Id;
        static auto shadowDepth(fqsm::Writing, system::Device::Id) -> resource_old::Material::Id;
    };

}
