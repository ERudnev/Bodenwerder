#pragma once

#include <base/maybe.h>
#include <eltanin/locality/thing.q1.h>
#include <eltanin/physics/rigid.q1.h>
#include <rmmr/math.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/texpack.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::locality {

    using namespace fqsm::api;

    struct Scrap : Feature<Scrap, Thing> {
        struct Resources {
            rmmr::resource::geometry::Asset::Id scrap;
            rmmr::resource::material::Asset::Id hull;
            rmmr::resource::texpack::Pack::Id mech;
        };
        struct Quantum {
            Custody<phys::rigid::Solid> body;
            Custody<rmmr::scene::actor::Mesh> actor;
        };
        struct Global {
            base::maybe<Resources> resources;
        };
        struct Actions : BaseActions {
            static void bindResources(Writing);
            static void update(Writing);
            static auto spawn(Writing, rmmr::Pose, vec3 halfExtents, float mass, vec3 linear, vec3 omega, float cohesion, phys::Kelvins temperature) -> Id;
            static void breakOff(Writing, vec3 worldCenter, quat worldRot, vec3 halfExtents, float mass, vec3 linear, float cohesion, phys::Kelvins temperature);
            static void followBody(Stewarding);
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions();
    };

}
