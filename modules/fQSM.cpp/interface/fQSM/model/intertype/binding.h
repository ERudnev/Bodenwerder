#pragma once

#include <string>
#include <string_view>

#include <base/shared_reference.h>

#include <fQSM/meta/interface.include.h>
#include <fQSM/model/_forwards.h>
#include <fQSM/processing/contexts/settingUp.h>
#include <fQSM/references.h>

namespace fqsm::model::intertype {

    // Per-aspect entry points of the runtime, bound once at schema registration.
    // Plain function pointers: every target is a template function or a capture-less lambda,
    // so std::function would only add an indirection on the normalization and merge loops.
    struct Binding {
        struct {
            ref<linear::patch::Erased> (*create)() = nullptr;
            void (*absorb)(complex::Patch&, const complex::Patch&) = nullptr;
            void (*clear)(complex::Patch&) = nullptr;
            std::string (*log)(const complex::Patch&, std::string_view aspectName) = nullptr;
        } patch;

        struct {
            ref<linear::state::Erased> (*create)() = nullptr;
            ref<linear::state::Erased> (*clone)(const complex::State&) = nullptr;
        } state;

        ref<linear::state::Erased> (*createFuture)(const complex::State&, ref<complex::Patch>) = nullptr;
        void (*integratePatchSlice)(complex::Reality&, const complex::Patch&) = nullptr;
        void (*mergePatchSlice)(const complex::State&, complex::Patch&, const complex::Patch&) = nullptr;
        void (*assemble)(::fqsm::processing::SettingUp&) = nullptr;
    };
}
