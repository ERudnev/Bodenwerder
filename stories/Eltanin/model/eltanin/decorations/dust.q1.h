#pragma once

#include <base/maybe.h>
#include <eltanin/physics/body.q1.h>
#include <rmmr/math.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/texpack.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::decorations {

    using namespace fqsm::api;

    struct Dust : Feature<Dust, rmmr::scene::actor::Mesh> {
        struct Resources {
            rmmr::resource::geometry::Asset::Id scrap;
            rmmr::resource::material::Asset::Id hull;
            rmmr::resource::texpack::Pack::Id mech;
        };
        struct Quantum {
            vec3 linear;
            vec3 omega;
            phys::Kelvins temperature;
            vec3 half;
            seconds life;
        };
        struct Global {
            base::maybe<Resources> resources;
        };
        struct Actions : BaseActions {
            static void bindResources(Writing);
            static void update(Writing, seconds dt);
            static auto spawn(Writing, rmmr::Pose, vec3 half, vec3 linear, vec3 omega, phys::Kelvins temperature) -> Id;
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

}
