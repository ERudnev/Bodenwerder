#pragma once

#include <functional>

#include <base/shared_reference.h>

#include <fQSM/meta/axis.h>
#include <fQSM/meta/interface.include.h>
#include <fQSM/references.h>

namespace fqsm::state::world {
    struct View;
    struct Data;
    struct Patch;
}

namespace fqsm::analysis {
    struct Patch;
}

namespace fqsm::schema {

    namespace axis = meta::axis;

    struct Binding {
        std::function<ref<state::slice::Erased>()> createState;
        std::function<ref<state::slice::Erased>()> createPatch;
        std::function<ref<state::slice::Erased>()> createDirtyVirtualPatch;
        std::function<ref<state::slice::Erased>(const state::world::Actual&)> cloneState;
        std::function<ref<state::slice::Erased>(const state::world::Actual&, const state::world::Patch&)> createOverlay;
        std::function<void(state::world::Data&, const state::world::Patch&)> integratePatchSlice;
        std::function<void(const state::world::View&, ref<state::world::Patch>, cref<state::world::Patch>)> mergePatchSlice;
        std::function<void(const state::world::Patch&, analysis::Patch&)> analyzePatchSlice;

        template<aspect::Any Meta>
        static Binding make();
    };
}