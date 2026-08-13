#pragma once

#include <rmmr/resources/manager.q1.h>

#include <fQSM/api/interface.h>

#include <eltanin/mech/blueprint.q1.h>

namespace eltanin::resource::blueprint {

    using namespace fqsm::api;

    // Construction schema on the Unit shelf. Payload = mech::Blueprint.
    // Files: assets/Eltanin/blueprints/*.blueprint
    struct Asset : Feature<Asset, rmmr::resource::Unit> {
        struct Quantum {
            mech::Blueprint data;
        };
        struct Actions : BaseActions {};
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Loader : Feature<Loader, Asset> {
        struct Quantum {
            filename file;
        };
        struct Actions : BaseActions {
            static void load(Writing, Id);
            static void save(Writing, Id);
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

}
