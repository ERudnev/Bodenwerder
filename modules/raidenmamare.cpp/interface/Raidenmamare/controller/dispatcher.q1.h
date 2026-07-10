#pragma once

#include <Raidenmamare/system/core.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::controller {

    using namespace fqsm::api;

    struct Dispatcher : Component<Dispatcher, system::Device> {
        struct Quantum {
            double clock;
            vector<bool> keys;
            index2 mouse;
        };
        struct Actions : BaseActions {
            static void update(Writing, Id, seconds now);
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
