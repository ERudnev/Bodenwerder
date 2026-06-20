#pragma once

#include <fQSM/processing/actions/merge.h>
#include <fQSM/model/complex/draft.h>
#include <fQSM/model/complex/patch.h>
#include <fQSM/processing/transaction.h>


namespace fqsm::processing::transaction {

    struct Branch : Transaction {
        Branch(Transaction& parent) : Branch(parent.childPolicy()) {}

        Branch(ChildPolicy policy)
            : patch(base::make_shared<model::complex::Patch>(policy.view.schema))
            , preview(policy.view, patch)
        {
            context = std::make_shared<Context>(Context{
                preview,
                patch,
                policy.upstream
            });
        }

        operator Reading() const override { return preview; }

    private:
        Context::Ptr context;
        Context::PatchRef patch;
        model::complex::Draft preview;

        auto writing() -> Writing override {
            return GateOperational{preview, context};
        }

        auto makeChildPolicy() -> ChildPolicy override {
            return ChildPolicy{
                preview,
                [this](Context::Result patch) { accept(patch); }
            };
        }

        void accept(Context::Result child) {
            actions::merge(preview, patch, child);
        }

    };
}