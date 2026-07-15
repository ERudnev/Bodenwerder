#pragma once

#include <rmmr/assets/geometry.q1.h>
#include <rmmr/resources_old/geometry.q1.h>
#include <rmmr/system/core.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::geometry {

    using namespace fqsm::api;

    struct GeometryGenerator final {
        static auto triangle(fqsm::Writing, system::Device::Id) -> resource_old::Geometry::Id;
        static auto kube(fqsm::Writing, system::Device::Id) -> resource_old::Geometry::Id;
        static auto gridPlane(fqsm::Writing, system::Device::Id) -> resource_old::Geometry::Id;
    };

}
