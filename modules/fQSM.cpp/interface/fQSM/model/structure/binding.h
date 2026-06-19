#pragma once

#include <functional>

#include <base/shared_reference.h>

#include <fQSM/meta/interface.include.h>
#include <fQSM/references.h>

namespace fqsm::model::structure {

    // TODO: replace with base::cannonball::composite::Lazy::Fabric Adapter
    struct Binding {
        // this is pack of linear state fabrics:
        std::function<ref<linear::state::Erased>()> createState;
        std::function<ref<linear::patch::Erased>()> createPatch;
        std::function<ref<linear::state::Erased>(const complex::State&)> cloneState;
        std::function<ref<linear::state::Erased>(const complex::State&, ref<complex::Patch>)> createDraft;
        std::function<void(complex::Reality&, const complex::Patch&)> integratePatchSlice;
        //std::function<void(const complex::State&, ref<complex::Patch>, cref<complex::Patch>)> mergePatchSlice;
        std::function<void(const complex::State&, complex::Patch&, const complex::Patch&)> mergePatchSlice;
        std::function<void(const complex::Patch&, analysis::Patch&)> analyzePatchSlice;

        template<aspect::Any Meta>
        static Binding make();
    };
}