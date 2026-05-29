#pragma once

#include <fQSM/state/overlay.h>
#include <fQSM/state/patch.h>
#include <fQSM/processing/transaction.h>


namespace fqsm::processing::transaction {

    struct Branch : Transaction {
        Branch(Permit& writing)
            : Transaction(writing)
            , patch(base::make_shared<state::world::Patch>(context->view.schema))
            , overlay(context->view, *patch)
        {
            auto parent = context;
    
            context = std::make_shared<Context>(
                overlay,
                patch,
                [parent](Context::PatchRef patch) {
                    if (parent->submit) {
                        (*parent->submit)(patch);
                        parent->submit = std::nullopt;
                    }
                }
            );
        }
    
        operator Reading() const override { return overlay; }
    
    private:
        Context::PatchRef patch;
        state::world::Overlay overlay;
    };
}