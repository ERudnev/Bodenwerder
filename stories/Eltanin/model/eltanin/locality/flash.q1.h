#pragma once

#include <eltanin/locality/thing.q1.h>
#include <eltanin/physics/body.q1.h>
#include <rmmr/math.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::locality {

    using namespace fqsm::api;

    struct Flash : Feature<Flash, Thing> {
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
            seconds elapsed;
        };
        struct Actions : BaseActions {
            static void update(Writing);
            static auto spawnAsExplosion(Writing, vec3 position, float strength) -> Id;
            static auto spawnAsThermal(Writing, vec3 position, float strength) -> Id;
            static void apply(Stewarding);
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions();
    };

}
