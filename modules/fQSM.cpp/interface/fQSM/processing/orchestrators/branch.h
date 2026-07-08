#pragma once

#include <fQSM/processing/algorithms/merge.h>
#include <fQSM/model/complex/future.h>
#include <fQSM/model/complex/patch.h>
#include <fQSM/processing/transaction.h>
#include <fQSM/processing/_forwards.h>
#include <fQSM/utility/logging.h>


namespace fqsm::processing::orchestrator {

    struct Branch : Transaction {
        Branch(Transaction& parent) : Branch(parent.childPolicy()) {}

        Branch(ChildPolicy policy) : context(std::make_shared<Context>(
            policy.view,
            base::make_shared<Patch>(policy.view.schema),
            policy.upstream
        ))
        {}

        operator Reading() const override { return context->world; }

    private:
        Context::Ptr context;

        auto writing() -> Writing override {
            return Gate(context);
        }

        auto makeChildPolicy() -> ChildPolicy override {
            return ChildPolicy{
                context->world,
                [this](Context::PatchRef patch) { accept(patch); }
            };
        }

        void accept(Context::PatchRef child) {
            _DBG_TX_("branch: merge child={} into accumulator={}", utility::format_patch(fqsm::freeze(child)), utility::format_patch(fqsm::freeze(context->accumulator)));
            algorithm::merge(context->world, context->accumulator, child);
        }

    };
}