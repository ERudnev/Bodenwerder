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
        umap<Primitive::Id, Primitive> knots;
        umap<Primitive::Id, Primitive> ribs;
        umap<Primitive::Id, Primitive> membranes;
        umap<Primitive::Id, Primitive> plates;
        umap<Primitive::Id, vector<Primitive>> volumes;
        vector<Primitive::Point> evaluatedParticles;
    };

}
