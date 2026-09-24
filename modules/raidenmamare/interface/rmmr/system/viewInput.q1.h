#pragma once

#include <rmmr/system/window.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::system {

    using namespace fqsm::api;

    struct ViewInput : Entity<ViewInput> {
        struct Quantum {
            bool engaged;
            vector<bool> keys;
            vector<bool> buttons;
            index2 mouseShift;
            float wheel;
        };
        struct Actions : BaseActions {
            static auto create(Writing) -> Id;
            static void engage(Writing, Id, bool);
            static void refresh(Writing, Window::Id);
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    // Schema fragment of doctrine/system/viewInput.q1: every aspect declared in this file.
    namespace doctrine {
        auto viewInput() -> Schema;
    }

}
