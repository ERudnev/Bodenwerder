#pragma once

#include "transaction.h"

namespace iqsm_mock::repo {

    struct Branch : Transaction {
        Branch(Reading reading) : Transaction(reading) {}
        Branch(Writing writing) : Transaction(std::move(writing)) {}
        ~Branch() {
            base::message("-repo::Base");
            finish();
        }

        World rebase(World world) {
            if (world == head.state) return head.state;
            head.state = operations::validate_smart(root, std::move(world));
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
            if (not head.upstream)
                return;
            head.upstream(delta());
            disconnect();
        }
    protected:
        void absorb(Delta delta) override {
            const auto before = head.state;
            head.state = operations::validate_smart(before, operations::integrate(head.state, std::move(delta)));
        }
    };
}
