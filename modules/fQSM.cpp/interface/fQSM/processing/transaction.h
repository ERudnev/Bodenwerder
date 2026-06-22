#pragma once

#include <memory>

#include <fQSM/model/_forwards.h>
#include <fQSM/processing/contexts/operational.h>

namespace fqsm::processing {

    struct Transaction {
        using Context = context::Operational;
        struct ChildPolicy {
            Reading view;
            Context::Upstream upstream;
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
