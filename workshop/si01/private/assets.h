#pragma once

#include <memory>

#include <fQSM/api/interface.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/sprites.q1.h>
#include <rmmr/system/core.q1.h>

namespace si01 {

    using namespace fqsm::api;

    struct Assets {
        rmmr::resource::sprite::Pack::Id kenney;
        rmmr::resource::geometry::Asset::Id unitQuad;

        static auto init(Writing, rmmr::system::Core::Id) -> std::unique_ptr<Assets>;
    };

}
