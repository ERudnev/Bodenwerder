#pragma once

#include <base/shared_reference.h>
#include <fQSM/erased/descriptor.h>
#include <fQSM/erased/patch_line.h>
#include <fQSM/meta/interface.include.h>
#include <fQSM/model/_forwards.h>


namespace fqsm::model::linear {

    template<category::Any Meta>
    struct Patch : patch::Erased {
        erased::PatchLine line{erased::ops_of<Quantum<Meta>>(), erased::ops_of<GlobalValue<Meta>>()};

        bool has_changes() const override { return line.has_changes(); }
        void absorb(const Patch& other) { line.absorb(other.line); }
        void clear() { line.clear(); }

        // schema
        static ref<patch::Erased> create() { return base::make_shared<Patch<Meta>>(); }
    };
}
