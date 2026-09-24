#pragma once

#include <base/maybe.h>
#include <fQSM/api/interface.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/texpack.q1.h>
#include <rmmr/wrapper/library.h>

namespace eltanin::assets {

    using namespace fqsm::api;

    struct Primitives {
        base::maybe<rmmr::resource::geometry::Asset::Id> grid;
        base::maybe<rmmr::resource::geometry::Asset::Id> sphere;
        base::maybe<rmmr::resource::geometry::Asset::Id> kube;
        base::maybe<rmmr::resource::geometry::Asset::Id> diamond;
    };

    struct Handles {
        Primitives primitive;
        base::maybe<rmmr::resource::geometry::Asset::Id> skySphereGeometry;
        base::maybe<rmmr::resource::geometry::Asset::Id> scrap;
        base::maybe<rmmr::resource::material::Asset::Id> skySphereMaterial;
        base::maybe<rmmr::resource::material::Asset::Id> skyBackdropMaterial;
        base::maybe<rmmr::resource::texpack::Pack::Id> sprites;
        base::maybe<rmmr::resource::material::Asset::Id> collisionDebugMaterial;
    };

    auto addCore(Writing, const rmmr::wrapper::assets::Handles&) -> base::maybe<Handles>;

}
