#pragma once

#include <fQSM/api/interface.h>

namespace placeholder {

    using namespace fqsm::api;

    // minimal possible Aspect, dont need *.cpp at all
    struct BoldEntity : Entity<BoldEntity> {
        struct Quantum {};
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct TriviaAttribute : Attribute<TriviaAttribute, BoldEntity> {
        struct Quantum {};
        struct Internals : DefaultInternals{};
    };

    struct MyAttribute : Attribute<MyAttribute, BoldEntity> {
        struct Always {
            static constexpr integer recommendedUpdateSize = 2;
        };
        struct Quantum {
            int x;
            int y;
        };

        struct Actions : BaseActions {
            static void justlog(Reading, Id);
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };
};