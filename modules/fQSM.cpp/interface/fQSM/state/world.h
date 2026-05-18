#pragma once

#include <stdexcept>
#include <utility>
#include <memory>

#include <fQSM/meta/concepts.h>
#include <fQSM/state/view.h>

namespace fqsm::state {

    struct World : View {
        struct Access {
            using Structure = Composite<axis::order::state, axis::mutability::writable>;

            template<aspect::Any Meta>
            using Slice = typename Structure::Entry::template TypedHandle<Meta>;
        };

        explicit World(Schema schema) : View(schema) {}

        Access::Structure composite;
    protected:
        auto slice(RAId runtimeTypeId) const -> cref<slice::Abstract<meta::axis::order::state>> override {
            return composite.slices.at(runtimeTypeId);
        }
    };
}