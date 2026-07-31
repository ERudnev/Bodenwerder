#pragma once

#include <eltanin/resources/physical.q1.h>
#include <rmmr/resources/runtimes.q1.h>

#include <fQSM/api/interface.h>

namespace eltanin::resource {

    using namespace fqsm::api;

    struct Assets : Component<Assets, rmmr::resource::Assets> {
        struct Quantum {};
        struct Actions : BaseActions {
            static auto add_physical_loader(Writing, rmmr::resource::Unit::Quantum, physical::Loader::Quantum) -> physical::Asset::Id;
            static void load(Writing);
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
