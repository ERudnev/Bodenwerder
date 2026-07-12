#pragma once

#include <rmmr/resources/shadowMap.q1.h>
#include <rmmr/system/core.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::shadow_map {


    struct ShadowMapGenerator {
        static auto create(fqsm::Writing context, system::Device::Id device, resource::index2 size) -> resource::ShadowMap::Id;
    };

}
