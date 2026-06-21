#pragma once

#include <functional>
#include <memory>
#include <base/logging.h>
#include <fQSM/model/_forwards.h>
#include <fQSM/model/complex/reality.h>
#include <fQSM/processing/_forwards.h>

namespace fqsm::processing {

    // Abstraction between "own" Draft and "someones"
    struct Context final {
        using Ptr = std::shared_ptr<Context>;
        // this may look not clear, but the truth is: Patch is mutable all ehe way through all systems
        using PatchRef = ref<model::complex::Patch>;
        using Upstream = std::function<void(PatchRef)>;

        const model::complex::State& view;
        PatchRef patch;
        Upstream upstream;

        ~Context() { finish(); } // _DEBUG_REPORT_;

        void finish() {
            if (upstream) {
                upstream(patch);
            }
        }
    };

    struct Gate {
        Gate(Context::Ptr parent) : context(std::move(parent)) {}

        operator Reading() const { return context->view; }
        const model::complex::State* operator->() const { return &context->view; }
        Context::PatchRef patch() { return context->patch; }

        //optimization stuff
        template<category::Any Meta>
        void expect_broad_update() {
            // TODO: enable this later when no necessary to include complex::Patch will be
            //patch->aspect<Meta>().items.reserve(view.aspect<Meta>().items().size());
        }
    private:
        // There is one fundamental problem around.
        // shared_ptr<Context> is quite ineffective for lightweinght Gate
        // but no other way to make transaction living as long as needed is visible
        // TODO: find better day to sovle this
        const Context::Ptr context;
    };

    template<category::Any Meta>
    struct Direct {
        using Container = base::cannonball::Table<Id<Meta>, Quantum<Meta>>;
        Direct(model::complex::Reality& reality)
            : items(static_cast<Container&>(reality.aspect<Meta>().items()))
            , full(reality)
        {}

        Container& items;

        operator Reading() const { return full; }

    private:
        const model::complex::State& full;

    };

}