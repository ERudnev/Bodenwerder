#pragma once

#include <base/maybe.h>
#include <eltanin/physics/body.q1.h>
#include <rmmr/math.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::decorations {

    using namespace fqsm::api;

    struct Dust : Feature<Dust, rmmr::scene::actor::Mesh> {
        struct Resources {
            rmmr::resource::geometry::Asset::Id scrap;
            rmmr::resource::material::Asset::Id glow;
            rmmr::resource::material::Asset::Id fade;
        };
        enum class Kind {
            thermal,
            kinetic,
        };
        struct Quantum {
            vec3 linear;
            vec3 omega;
            phys::Kelvins temperature;
            phys::Kelvins bornTemperature;
            vec3 half;
            float mass;
            seconds life;
            seconds lifeBorn;
            Kind kind;
            float fadeFrom;
            float fadeTo;
        };
        struct Global {
            base::maybe<Resources> resources;
        };
        struct Actions : BaseActions {
            static void bindResources(Writing);
            static void update(Writing, seconds dt);
            static auto spawn(Writing, rmmr::Pose, vec3 half, vec3 linear, vec3 omega, phys::Kelvins temperature) -> Id;
            static auto spawnKinetic(Writing, rmmr::Pose, vec3 half, vec3 linear, vec3 omega) -> Id;
            static auto spawnMesh(Writing, rmmr::Pose, vector<rmmr::scene::actor::Mesh::Occurrence>, vec3 linear, vec3 omega, phys::Kelvins temperature, vec3 half, float latticeStep) -> Id;
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

}
