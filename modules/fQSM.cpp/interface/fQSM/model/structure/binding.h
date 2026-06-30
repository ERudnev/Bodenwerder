#pragma once

#include <functional>
#include <string>
#include <string_view>

#include <base/shared_reference.h>

#include <fQSM/meta/interface.include.h>
#include <fQSM/model/_forwards.h>
#include <fQSM/references.h>

namespace fqsm::model::structure {

    // TODO: replace with base::cannonball::composite::Lazy::Fabric Adapter
    struct Binding {
        // this is pack of linear state fabrics:
        struct {
            std::function<ref<linear::patch::Erased>()> create;
            std::function<void(complex::Patch&, const complex::Patch&)> absorb;
            std::function<void(complex::Patch&)> clear;
            std::function<std::string(const complex::Patch&, std::string_view aspectName)> log;
        } patch;

        struct {
            std::function<ref<linear::state::Erased>()> create;
            std::function<ref<linear::state::Erased>(const complex::State&)> clone;
        } state;

        std::function<ref<linear::state::Erased>(const complex::State&, ref<complex::Patch>)> createFuture;
        std::function<void(complex::Reality&, const complex::Patch&)> integratePatchSlice;
        //std::function<void(const complex::State&, ref<complex::Patch>, cref<complex::Patch>)> mergePatchSlice;
        std::function<void(const complex::State&, complex::Patch&, const complex::Patch&)> mergePatchSlice;
    };
}