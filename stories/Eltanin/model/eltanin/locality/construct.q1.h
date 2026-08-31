#pragma once

#include <base/maybe.h>
#include <eltanin/locality/thing.q1.h>
#include <eltanin/mech/construction.q1.h>
#include <eltanin/mech/semantics.q1.h>
#include <eltanin/physics/rigid.q1.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/meshpack.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::locality {

    using namespace fqsm::api;

    struct Construct : Feature<Construct, Thing> {
        struct ActorFragments {
            struct OfKnot {
                mech::Construction::Primitive::Id knot;
                mech::skeleton::Corner quark;
            };
            struct OfRib {
                mech::Construction::Primitive::Id rib;
                mech::skeleton::Halfrib quark;
                index3 at;
            };
            struct OfMembrane {
                mech::Construction::Primitive::Id membrane;
                mech::skeleton::Membrane quark;
                index3 at;
            };
            struct OfPlate {
                mech::Construction::Primitive::Id plate;
                rmmr::resource::Unit::Name mount;
                mech::space::Transform transform;
            };
            struct OfVolume {
                mech::Construction::Primitive::Id volume;
                rmmr::resource::Unit::Name mount;
                mech::space::Transform transform;
            };
            vector<OfKnot> ofKnot;
            vector<OfRib> ofRib;
            vector<OfMembrane> ofMembrane;
            vector<OfPlate> ofPlate;
            vector<OfVolume> ofVolume;
        };
        struct Resources {
            rmmr::resource::meshpack::Asset::Id interframe;
        };
        struct Quantum {
            Custody<phys::rigid::Crystal> body;
            Custody<rmmr::scene::actor::Mesh> actor;
            ActorFragments fragments;
            mech::Construction construction;
            vector<mech::Construction::Primitive::Id> visualOf;
        };
        struct Global {
            base::maybe<Resources> resources;
        };
        struct Actions : BaseActions {
            static void bindResources(Writing);
            static void update(Writing);
            static void shedDead(Writing);
            static void followBody(Stewarding);
            static void syncVisualCohesion(Reading, Id);
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions();
    };

}
