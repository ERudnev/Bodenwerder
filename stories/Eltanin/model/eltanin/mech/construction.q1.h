#pragma once

#include <eltanin/mech/semantics.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::mech {

    using namespace fqsm::api;

    struct Construction {
        struct Primitive {
            using Id = integer;
            struct Point {
                index3 gridPos;
                float mass;
            };
            struct Welded : Point {
                float strength;
            };
            vector<Welded> loop;
            float thickness;
        };
        struct Knot : Primitive {
            vector<integer> welded;
        };
        map<Primitive::Id, Knot> knots;
        map<Primitive::Id, Primitive> ribs;
        map<Primitive::Id, Primitive> membranes;
        map<Primitive::Id, Primitive> plates;
        map<Primitive::Id, vector<Primitive>> volumes;
        vector<Primitive::Point> evaluatedParticles;
    };

}
