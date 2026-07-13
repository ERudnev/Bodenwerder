#pragma once

#include <rmmr/system/core.q1.h>

#include <fQSM/api/interface.h>

struct ImGuiContext;

namespace rmmr::system {

    using namespace fqsm::api;

    struct ImGuiHost : Component<ImGuiHost, Device> {
        using Context = ImGuiContext*;

        struct Quantum {
            Context context;
        };
        struct Actions : BaseActions {
            static void initialize(Writing, Id);
            static void newFrame(Reading, Id);
            static void render(Reading, Id);
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

}
