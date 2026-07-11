#pragma once

#include <Raidenmamare/system/core.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::system {

    using namespace fqsm::api;

    struct Window : Component<Window, Device> {
        using time = timepoint;

        struct InputState {
            integer frame;
            time clock;
            vector<bool> keys;
            index2 mouse;
        };

        struct Quantum {
            string title;
            index2 size;
            InputState previous;
            InputState current;
        };
        struct Actions : BaseActions {
            static auto create(Writing, string title, index2 size) -> Id;
            static void present(Reading, Id);
            static auto dt(Reading, Id) -> seconds;
            static auto mouseShift(Reading, Id) -> index2;
            static void onFrameAdvanced(Writing, Id);
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
