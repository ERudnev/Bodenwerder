#pragma once

#include <iQSM/flow/processing.h>
#include <iQSM/flow/transaction.h>
#include <iQSM/state/view.h>


namespace iqsm::flow {

    struct Branch : Transaction {
        Branch(Reading reading) : Transaction(reading->share()) {}
        Branch(Writing writing) : Transaction(std::move(writing)) {}
        explicit Branch(Transaction& parent) : Transaction(parent) {}
        explicit Branch(Branch& parent) : Transaction(parent) {}
        ~Branch() {
            on_finish();
        }

        Reading rebase(Reading world) {
            if (world == head.state) return head.state;
            const auto before = head.state;
            head.state = world;

            flow::validateSmart(cleanChannel(), before);
            root = head.state;
            return root;
        }

        Delta delta() const { return flow::makeDelta(root, head.state); }

        Delta push() {
            auto out = delta();
            root = head.state;
            return out;
        }

    protected:
        /// Writing for nested work (e.g. validate_smart) without re-entering `absorb` → validate_smart.
        Writing cleanChannel() {
            return permit_with_upstream([this](Channel::Result result) {
                if (result.maybeState.exists()) {
                    head.state = *std::move(result.maybeState);
                    return;
                }
                if (result.delta->empty()) return;
                head.state = flow::integrate(head.state, std::move(result.delta));
            });
        }

        void on_finish() override {
            if (unwinding()) return;
            if (not head.upstream)
                return;
            head.upstream({head.state, delta()});
            disconnect();
        }
        void absorb(Channel::Result result) override {
            const auto before = head.state;
            if (result.maybeState.exists()) {
                head.state = *std::move(result.maybeState);
            } else {
                head.state = flow::integrate(head.state, result.delta);
            }
            flow::validateSmart(cleanChannel(), before);
        }
    };
}
