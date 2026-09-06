#pragma once

#include <base/maybe.h>
#include <eltanin/mech/semantics.q1.h>
#include <rmmr/math.q1.h>
#include <rmmr/resources/manager.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::mech {

    using namespace fqsm::api;

    // Footprint in discrete local space (LW-authored lattice points). No plate-shape composition.
    struct Attachment {
        std::vector<base::common_types::index3> points;

        // All attachment points on one lattice plane (incl. ≤2 / collinear).
        auto flatMounted() const -> bool;
    };

    struct LatticeHull {
        float thickness;
        vector<vector<integer>> faces;
    };

    struct Box {
        vec3 min;
        vec3 max;
    };

    struct Element {
        std::string name;
        base::maybe<LatticeHull> latticeHull;
        base::maybe<Box> box;
    };

    // Library entry: placeable equipment.
    // Files: assets/Eltanin/fittings/<shelf>/*.json → Eltanin::<shelf>.<stem>
    struct Mount : Feature<Mount, rmmr::resource::Unit> {
        struct PresentationGeometry {
            rmmr::resource::Unit::Name pack;
            std::string entry;
        };
        struct Quantum {
            std::string name;
            std::string author;
            float mass;
            Attachment attachment;
            vector<Element> elements;
            vector<PresentationGeometry> presentationGeometry;
            base::maybe<Role> role;
            filename file; // kit-relative; under fittings/<shelf>/
        };
        struct Actions : BaseActions {
            static void load(Writing, Id);
            static void save(Writing, Id);
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

}
