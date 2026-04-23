#pragma once

#include <Raidenmamare/primitives/base.q1.h>

#include <iQSM/api/_gateway.h>

namespace rmmr::primitive {

    using namespace iqsm::q1_gateway;

    struct MeshGenerator final {
        // Declares a canonical triangle mesh resource in the repository.
        //
        // The commit receiver is expected to apply the delta into a repo object
        // (Branch/Sequence/Accumulator), so the resulting Id becomes observable by the caller.
        static auto triangle(Writing, rmmr::Device::Id, resources::Manager) -> Base::Id;
        static auto kube(Writing, rmmr::Device::Id, resources::Manager) -> Base::Id;
        static auto gridPlane(Writing, rmmr::Device::Id, resources::Manager) -> Base::Id;
    };

}

