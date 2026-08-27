#pragma once

#include <eltanin/mech/semantics.q1.h>
#include <eltanin/physics/rigid.q1.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::mech {

    using namespace fqsm::api;

    struct Construction {
        struct Knot {
            using Id = integer;
            index3 position;
        };
        struct Rib {
            using Id = integer;
            index3 start;
            index3 end;
        };
        struct Tile {
            using Id = integer;
            vector<index3> loop;
        };
        struct Plate {
            using Id = integer;
            vector<index3> loop;
            float thickness;
        };
        struct Weld4Rib {
            Knot::Id knot;
            Rib::Id rib;
        };
        umap<Knot::Id, Knot> knots;
        umap<Rib::Id, Rib> ribs;
        umap<Tile::Id, Tile> tiles;
        umap<Plate::Id, Plate> plates;
        vector<Weld4Rib> weld4rib;
    };

    struct Construct : Entity<Construct> {
        struct ActorFragments {
            struct OfKnot {
                Construction::Knot::Id knot;
                skeleton::Corner quark;
            };
            struct OfRib {
                Construction::Rib::Id rib;
                skeleton::Halfrib quark;
                index3 at;
            };
            struct OfMembrane {
                Construction::Tile::Id tile;
                skeleton::Membrane quark;
                index3 at;
            };
            struct OfPlate {
                Construction::Plate::Id plate;
                rmmr::resource::Unit::Name mount;
                space::Transform transform;
            };
            vector<OfKnot> ofKnot;
            vector<OfRib> ofRib;
            vector<OfMembrane> ofMembrane;
            vector<OfPlate> ofPlate;
        };
        struct Quantum {
            Custody<phys::rigid::Crystal> body;
            Custody<rmmr::scene::actor::Mesh> actor;
            ActorFragments fragments;
            Construction construction;
        };
        struct Actions : BaseActions {};
        struct Internals;
        static const Behavior customAspectReactions();
    };

}
