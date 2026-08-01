#pragma once

#include <rmmr/math.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/manager.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::resource::atomic {

    using namespace fqsm::api;

    using Reference = rmmr::resource::Unit::Reference;

    struct Asset : Feature<Asset, rmmr::resource::Unit> {
        struct Face {
            string name;
            vector<integer> indices;
        };
        struct Quantum {
            vector<rmmr::Pos> points;
            vector<Face> faces;
            rmmr::resource::geometry::Asset::Id visualizer;
        };
        struct Actions : BaseActions {};
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
