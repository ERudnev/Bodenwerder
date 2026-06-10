#pragma once

#include <memory>

#include <fQSM/state/_forwards.h>
#include <fQSM/processing/commit.h>

namespace fqsm::processing {

    struct Transaction {
        struct ChildPolicy {
            Reading view;
            Commit::Upstream upstream;
        };

        virtual ~Transaction() = default;

        virtual operator Reading() const = 0;

        operator Writing() {
            return writing();
        }

        auto childPolicy() -> ChildPolicy {
            return makeChildPolicy();
        }

    protected:
        virtual auto writing() -> Writing = 0;
        virtual auto makeChildPolicy() -> ChildPolicy = 0;
    };
}
