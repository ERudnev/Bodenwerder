#pragma once

#include <rmmr/math.q1.h>
#include <rmmr/resources/manager.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::resource::physical {

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
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Loader : Feature<Loader, Asset> {
        struct Quantum {
            filename file;
        };
        struct Actions : BaseActions {
            static void load(Writing, Id);
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
