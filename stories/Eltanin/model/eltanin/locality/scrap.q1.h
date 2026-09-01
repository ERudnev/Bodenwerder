#pragma once

#include <base/maybe.h>
#include <eltanin/locality/thing.q1.h>
#include <eltanin/physics/body.q1.h>
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
            rmmr::resource::material::Asset::Id wreck;
            rmmr::resource::texpack::Pack::Id mech;
        };
        enum class Lineage {
            common,
            volume,
            terminal,
        };
        struct Quantum {
            Custody<phys::rigid::Solid> body;
            Custody<rmmr::scene::actor::Mesh> actor;
            phys::Kelvins gpuKelvin;
            rmmr::Pose meshFromBody;
            Lineage lineage;
        };
        struct Global {
            base::maybe<Resources> resources;
        };
        struct Actions : BaseActions {
            static void bindResources(Writing);
            static void update(Writing);
            static auto spawn(Writing, rmmr::Pose, vec3 halfExtents, float mass, vec3 linear, vec3 omega, float cohesion, phys::Kelvins temperature, Lineage, base::maybe<phys::Body::Id> cohort = {}) -> Id;
            static auto spawnMesh(Writing, rmmr::Pose actorPose, rmmr::Pose bodyPose, vec3 halfExtents, float mass, vec3 linear, vec3 omega, float cohesion, phys::Kelvins temperature, vector<rmmr::scene::actor::Mesh::Occurrence>, float latticeStep, Lineage, base::maybe<phys::Body::Id> cohort = {}) -> Id;
            static auto cutCount(float cohesion) -> int;
            static void breakOff(Writing, vec3 worldCenter, quat worldRot, vec3 halfExtents, float mass, vec3 linear, float cohesion, phys::Kelvins temperature, base::maybe<phys::Body::Id> cohort = {});
            static void radiate(Stewarding, seconds dt);
            static void followBody(Stewarding);
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions();
    };

}
