#pragma once

#include <unordered_map>
#include <optional>

#include <base/maybe.h>
#include <fQSM/typeId.h>
#include <fQSM/meta/axis.h>
#include <fQSM/meta/alias.h>
#include <fQSM/meta/concepts.h>

namespace fqsm::meta::state {
    template<aspect::Any, axis::order>
    struct ItemsLayout;
}


namespace fqsm::meta::state {

    // Items Layout specs:
    template<aspect::Any Meta>
    struct ItemsLayout<Meta, axis::order::state> {
        using Element = Quantum<Meta>;
    };

    template<aspect::Any Meta>
    struct ItemsLayout<Meta, axis::order::patch> {
        using Element = base::maybe<Quantum<Meta>>;
    };

    // Differentiation (converting state <-> patch):
    template<aspect::Any Meta>
    struct Differentiation {
        using State = typename Meta::Runtime::Element::State;
        using Patch = typename Meta::Runtime::Element::Patch;

        static Patch add(State after) { return Patch{std::move(after)}; }
        static Patch change(State after) { return Patch{std::move(after)}; }
    };
}


