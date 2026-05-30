#pragma once

#include <fQSM/state/overlay.h>
#include <fQSM/state/patch.h>
#include <fQSM/processing/transaction.h>


namespace fqsm::processing::transaction {

    struct Branch : Transaction {
        Branch(Transaction& parent) : Branch(parent.childPolicy()) {}

        Branch(ChildPolicy policy)
            : patch(base::make_shared<state::world::Patch>(policy.view.schema))
            , overlay(policy.view, *patch)
        {    
            context = std::make_shared<Context>(Context{
                overlay,
                patch,
                policy.upstream
            });
        }
    
        operator Reading() const override { return overlay; }

    private:
        ContextShared context;
        Context::PatchRef patch;
        state::world::Overlay overlay;

        auto writing() -> Writing override {
            return Gate{overlay, context};
        }

        auto makeChildPolicy() -> ChildPolicy override {
            return ChildPolicy{
                overlay,
                [this](Context::PatchRef patch) { accept(patch); }
            };
        }

        void accept(Context::PatchRef) {
            _INCOMPLETE_;
        }

    };
}