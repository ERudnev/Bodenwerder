#pragma once

#include <eltanin/mech/semantics.q1.h>
#include <rmmr/resources/manager.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::mech {

    using namespace fqsm::api;

    // Construction schema on the Unit shelf.
    // Files: assets/Eltanin/blueprints/*.blueprint
    struct Blueprint : Feature<Blueprint, rmmr::resource::Unit> {
        struct Cell {
            skeleton::Placement placement;
            frame::shape shape; // declared intent; population may be thinned
            std::vector<skeleton::Corner> corners;
            std::vector<skeleton::Halfrib> halfribs;
            std::vector<skeleton::Membrane> membranes;
        };
        struct Mounted {
            rmmr::resource::Unit::Name mount;
            space::Transform transform;
        };
        struct Quantum {
            std::string name;
            std::string author; // manufacturer
            std::vector<Cell> cells;
            std::vector<Mounted> mounts;
            filename file; // kit-relative; under blueprints/
        };
        struct Actions : BaseActions {
            static void load(Writing, Id);
            static void save(Writing, Id);
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

}
