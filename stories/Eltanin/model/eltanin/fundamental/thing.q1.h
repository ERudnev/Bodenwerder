#pragma once

#include <fQSM/api/interface.h>

namespace eltanin::fundamental {

    using namespace fqsm::api;

    struct Thing : Entity<Thing> {
        struct Quantum {};
        struct Global {
            seconds now;
        };
        struct Civil {
            integer year;
            integer month;
            integer day;
            integer hour;
            integer minute;
            integer second;
        };
        struct Always {
            static auto setup(SettingUp&) -> Global;
            static auto civil(seconds now) -> Civil;
        };
        struct Actions : BaseActions {
            static void update(Writing, seconds dt);
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

    namespace doctrine {
        auto thing() -> Schema;
    }

}
