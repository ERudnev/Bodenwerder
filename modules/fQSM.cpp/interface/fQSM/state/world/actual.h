#pragma once

#include <fQSM/state/composite.h>
#include <fQSM/state/slice/actual.h>

namespace fqsm::state::world {

    struct Actual {
        virtual ~Actual() = default;
        const Schema schema;


    protected:
        explicit Actual(Schema schema) : schema{std::move(schema)} {}
    };

    struct

}