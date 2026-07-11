#pragma once

#include <Raidenmamare/system/window.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::system {

    using namespace fqsm::api;

    struct Viewport : Entity<Viewport> {
        struct Quantum {
            index2 origin;
            index2 size;
            vec4 clear_color;
        };
        struct Actions : BaseActions {
            static void activate(Reading, Id);
            static void clear(Reading, Id);
            static void syncExtent(Writing, Id);
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Viewport_group : Group<Viewport_group, Window, Viewport> {
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
