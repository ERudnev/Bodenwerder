#pragma once

#include <fQSM/processing/actions/merge.h>
#include <fQSM/state/world/preview.h>
#include <fQSM/state/patch.h>
#include <fQSM/processing/transaction.h>


namespace fqsm::processing::transaction {

    struct Branch : Transaction {
        Branch(Transaction& parent) : Branch(parent.childPolicy()) {}

        Branch(ChildPolicy policy)
            : patch(base::make_shared<state::world::Patch>(policy.view.schema))
            , preview(policy.view, *patch)
        {
            context = std::make_shared<Commit>(Commit{
                preview,
                patch,
                policy.upstream
            });
        }

        operator Reading() const override { return preview; }

    private:
        ContextShared context;
        Commit::PatchRef patch;
        state::world::Preview preview;

        auto writing() -> Writing override {
            return Gate{preview, context};
        }

        auto makeChildPolicy() -> ChildPolicy override {
            return ChildPolicy{
                preview,
                [this](Commit::PatchRef patch) { accept(patch); }
            };
        }

        void accept(Commit::PatchRef child) {
            actions::merge(preview, patch, child);
        }

    };
}