#pragma once

#include <memory>

#include <fQSM/model/complex/patch.h>
#include <fQSM/processing/algorithms/merge.h>
#include <fQSM/processing/transaction.h>

namespace fqsm::processing::orchestrator {

    // A patch over the parent's state. It does not normalize: when it closes, the parent takes the patch.
    struct Branch : Transaction {
        explicit Branch(Transaction& parent)
            : parent(&parent)
            , session(std::make_unique<Session>(parent.child_base(), base::make_shared<model::complex::Patch>(parent.child_base())))
        {}
        // a Branch of a Branch nests (it is not a copy)
        explicit Branch(Branch& parent) : Branch(static_cast<Transaction&>(parent)) {}
        Branch(Branch&&) noexcept = default;
        Branch& operator=(Branch&&) = delete;
        ~Branch() override { if (session) parent->accept_child(session->view.patch()); }

        operator Reading() const override { return Reading(session->view); }

    private:
        Transaction* parent;
        std::unique_ptr<Session> session;

        auto writing(Mode) -> Writing override { return Writing(*session); }
        auto child_base() const -> const model::complex::State& override { return session->view; }
        // a refused child is discarded alone; its messages stay as warnings of this branch
        void accept_child(ref<model::complex::Patch> child) override {
            if (child->summary.good())
                return algorithm::merge(session->view, session->view.patch(), child);
            auto& warnings = session->view.summary().warning;
            warnings.insert(warnings.end(), child->summary.critical.begin(), child->summary.critical.end());
        }
    };
}
