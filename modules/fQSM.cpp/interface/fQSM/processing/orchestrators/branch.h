#pragma once

#include <cassert>
#include <memory>
#include <stdexcept>

#include <fQSM/erased/line.h>
#include <fQSM/model/complex/patch.h>
#include <fQSM/processing/algorithms/merge.h>
#include <fQSM/processing/transaction.h>

namespace fqsm::processing::orchestrator {

    // A patch over the parent's state. It does not normalize: when it closes, the parent takes the patch.
    struct Branch : Transaction {
        // throws std::logic_error when the nesting exceeds the layers a cursor can stack
        explicit Branch(Transaction& parent)
            : parent(&parent)
            , session(open(parent))
        {
            depth = parent.depth + 1;
        }
        // a Branch of a Branch nests (it is not a copy)
        explicit Branch(Branch& parent) : Branch(static_cast<Transaction&>(parent)) {}
        Branch(Branch&&) noexcept = default;
        Branch& operator=(Branch&&) = delete;
        // an exception that unwinds the Branch discards its patch
        ~Branch() override {
            if (not session) return;
            assert(not session->has_handles() and "fQSM: a handle outlives its Branch");
            if (session->unwinding()) return;
            parent->accept_child(session->view.patch());
        }

        operator Reading() const override { return Reading(session->view); }

    private:
        Transaction* parent;
        std::unique_ptr<Session> session;

        static auto open(Transaction& parent) -> std::unique_ptr<Session> {
            if (parent.depth >= erased::Cursor::MaxLayers)
                throw std::logic_error("fQSM: Branch nesting exceeds the cursor layer limit");
            return std::make_unique<Session>(parent.child_base(), base::make_shared<model::complex::Patch>(parent.child_base()));
        }

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
