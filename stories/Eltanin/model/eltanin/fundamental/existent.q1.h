#pragma once

#include <fQSM/api/interface.h>

namespace eltanin::fundamental {

    using namespace fqsm::api;

    struct Existent : Entity<Existent> {
        struct Quantum {};
        struct Global {
            seconds now;
            integer warp;
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
            static constexpr integer warpTop = 8;
            static auto assemble(SettingUp&) -> Global;
            static auto rate(integer warp) -> seconds;
            static auto civil(seconds now) -> Civil;
            static auto warpLabel(integer warp) -> const char*;
        };
        struct Actions : BaseActions {
            static void update(Writing, seconds dt);
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

    namespace doctrine {
        auto existent() -> Schema;
    }

}
