#pragma once

#include <base/maybe.h>
#include <eltanin/locality/thing.q1.h>
#include <eltanin/physics/body.q1.h>
#include <rmmr/math.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::locality {

    using namespace fqsm::api;

    struct Flash : Feature<Flash, Thing> {
        struct Resources {
            rmmr::resource::geometry::Asset::Id sphere;
            rmmr::resource::material::Asset::Id flash;
            rmmr::resource::material::Asset::Id flashGlow;
        };
        struct Effect {
            struct Kinetic {
                float strength;
                float radius;
                float core;
                seconds duration;
            };
            struct Thermal {
                phys::Kelvins temperature;
                float energy;
                float radius;
                seconds duration;
            };
            struct Fracture {
                float yield;
                float radius;
            };
            Kinetic kinetic;
            Thermal thermal;
            Fracture fracture;
        };
        struct Quantum {
            Effect effect;
            Custody<rmmr::scene::actor::Mesh> shock;
            Custody<rmmr::scene::actor::Mesh> plasma;
            vec3 linear;
            seconds elapsed;
        };
        struct Global {
            base::maybe<Resources> resources;
        };
        struct Actions : BaseActions {
            static void bindResources(Writing);
            static void update(Writing);
            static auto spawnAsExplosion(Writing, vec3 position, vec3 linear, float strength) -> Id;
            static auto spawnAsThermal(Writing, vec3 position, vec3 linear, float strength) -> Id;
            static void apply(Stewarding);
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions();
    };

}
