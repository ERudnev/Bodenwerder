#pragma once

#include <rmmr/math.q1.h>
#include <rmmr/resources/manager.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::resource::physical {

    using namespace fqsm::api;

    using Reference = resource::Unit::Reference;

    struct Asset : Feature<Asset, resource::Unit> {
        struct Face {
            string name;
            vector<integer> indices;
        };
        struct Quantum {
            vector<Pos> points;
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
