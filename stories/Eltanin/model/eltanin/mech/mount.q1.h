#pragma once

#include <eltanin/mech/semantics.q1.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/meshpack.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::mech {

    using namespace fqsm::api;

    // External mount footprint on plate sockets (armor / hardpoints).
    struct Socket {
        struct Element {
            plate::shape shape;
            Pose pose;
        };
        std::vector<Element> profile;
    };

    // Library entry: placeable external equipment. Visual = soft meshpack link (placeholder).
    struct Mount : Feature<Mount, rmmr::resource::Unit> {
        // Soft link: pack Unit::Reference (id + Name backup) + entry key in meshpack::Asset.entries.
        struct TempMesh {
            rmmr::resource::meshpack::Reference pack;
            std::string entry;
        };
        struct Quantum {
            std::string name;
            std::string author;
            Socket socket;
            TempMesh tempMesh;
        };
        struct Actions : BaseActions {};
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

}
