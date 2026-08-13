#pragma once

#include <eltanin/mech/semantics.q1.h>
#include <rmmr/resources/manager.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::mech {

    using namespace fqsm::api;

    // Footprint in discrete local space (LW-authored lattice points). No plate-shape composition.
    struct Attachment {
        std::vector<base::common_types::index3> points;
    };

    // Library entry: placeable external equipment. Visual = soft meshpack link (placeholder).
    // Files: assets/Eltanin/fittings/mounts/*.json
    struct Mount : Feature<Mount, rmmr::resource::Unit> {
        struct TempMesh {
            rmmr::resource::Unit::Name pack;
            std::string entry;
        };
        struct Quantum {
            std::string name;
            std::string author;
            Attachment attachment;
            TempMesh tempMesh;
            filename file; // kit-relative; under fittings/mounts/
        };
        struct Actions : BaseActions {
            static void load(Writing, Id);
            static void save(Writing, Id);
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

}
