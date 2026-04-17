#pragma once

#include <iQSM/operations/integration.h>
#include <iQSM/repository/transaction.h>

namespace iqsm::repo {

    struct Branch : Transaction {
        Branch(Reading reading) : Transaction(reading) {}
        Branch(Writing writing) : Transaction(std::move(writing)) {}
        ~Branch() {
            finish();
        }

        World rebase(World world) {
            if (world == head.state) return head.state;
            const auto before = head.state;
            head.state = operations::validate_smart(before, world);
            root = head.state;
            return root;
        }

        Delta delta() const { return operations::make_delta(root, head.state); }

        Delta push() {
            auto out = delta();
            root = head.state;
            return out;
        }

        void finish() override {
            if (unwinding()) return;
            if (not head.upstream)
                return;
            head.upstream(delta());
            disconnect();
        }
    protected:
        void absorb(Delta delta) override {
            const auto before = head.state;
            head.state = operations::validate_smart(before, operations::integrate(head.state, delta));
        }
    };
}
