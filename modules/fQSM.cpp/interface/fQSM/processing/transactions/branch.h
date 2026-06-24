#pragma once

#include <fQSM/processing/actions/merge.h>
#include <fQSM/model/complex/future.h>
#include <fQSM/model/complex/patch.h>
#include <fQSM/processing/transaction.h>


namespace fqsm::processing::transaction {

    struct Branch : Transaction {
        Branch(Transaction& parent) : Branch(parent.childPolicy()) {}

        Branch(ChildPolicy policy) : context(std::make_shared<Context>(
            policy.view,
            base::make_shared<Patch>(policy.view.schema),
            base::cannonball::SeeChanges::observable,
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
            actions::merge(context->world, context->accumulator, child);
        }

    };
}