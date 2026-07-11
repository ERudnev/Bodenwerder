#pragma once

#include <Raidenmamare/assets/geometry.q1.h>
#include <Raidenmamare/resources/geometry.q1.h>
#include <Raidenmamare/system/core.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::geometry {

    using namespace fqsm::api;

    struct GeometryGenerator final {
        static auto triangle(fqsm::Writing, system::Device::Id) -> resource::Geometry::Id;
        static auto kube(fqsm::Writing, system::Device::Id) -> resource::Geometry::Id;
        static auto gridPlane(fqsm::Writing, system::Device::Id) -> resource::Geometry::Id;
    };

}
