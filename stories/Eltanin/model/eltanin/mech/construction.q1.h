#pragma once

#include <eltanin/mech/semantics.q1.h>
#include <eltanin/physics/rigid.q1.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::mech {

    using namespace fqsm::api;

    struct Construction {
        struct Primitive {
            using Id = integer;
            struct Point {
                index3 gridPos;
                float mass;
                integer particle;
            };
            struct Welded : Point {
                float strength;
            };
            vector<Welded> loop;
            float thickness;
        };
        umap<Primitive::Id, Primitive> knots;
        umap<Primitive::Id, Primitive> ribs;
        umap<Primitive::Id, Primitive> membranes;
        umap<Primitive::Id, Primitive> plates;
        umap<Primitive::Id, vector<Primitive>> volumes;
        vector<Primitive::Point> evaluatedParticles;
    };

    struct Construct : Entity<Construct> {
        struct ActorFragments {
            struct OfKnot {
                Construction::Primitive::Id knot;
                skeleton::Corner quark;
            };
            struct OfRib {
                Construction::Primitive::Id rib;
                skeleton::Halfrib quark;
                index3 at;
            };
            struct OfMembrane {
                Construction::Primitive::Id membrane;
                skeleton::Membrane quark;
                index3 at;
            };
            struct OfPlate {
                Construction::Primitive::Id plate;
                rmmr::resource::Unit::Name mount;
                space::Transform transform;
            };
            struct OfVolume {
                Construction::Primitive::Id volume;
                rmmr::resource::Unit::Name mount;
                space::Transform transform;
            };
            vector<OfKnot> ofKnot;
            vector<OfRib> ofRib;
            vector<OfMembrane> ofMembrane;
            vector<OfPlate> ofPlate;
            vector<OfVolume> ofVolume;
        };
        struct Quantum {
            Custody<phys::rigid::Crystal> body;
            Custody<rmmr::scene::actor::Mesh> actor;
            ActorFragments fragments;
            Construction construction;
            vector<Construction::Primitive::Id> visualOf;
        };
        struct Actions : BaseActions {
            static void syncVisualCohesion(Reading, Id);
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

}
