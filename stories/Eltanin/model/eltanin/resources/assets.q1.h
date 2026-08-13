#pragma once

#include <eltanin/mech/blueprint.q1.h>
#include <rmmr/resources/runtimes.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::resource {

    using namespace fqsm::api;

    struct Assets : Component<Assets, rmmr::resource::Assets> {
        struct Quantum {};
        struct Actions : BaseActions {
            static auto add_blueprint(Writing, rmmr::resource::Unit::Name, filename) -> mech::Blueprint::Id;
        };
        struct Internals : DefaultInternals {};
        static const Behavior customAspectReactions() { return {}; }
    };

}
