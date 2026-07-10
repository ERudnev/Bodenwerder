#pragma once

#include <Raidenmamare/system/core.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::system {

    using namespace fqsm::api;

    struct Window : Component<Window, Device> {
        struct Quantum {
            string title;
            index2 size;
        };
        struct Actions : BaseActions {
            static void present(Reading, Id);
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
