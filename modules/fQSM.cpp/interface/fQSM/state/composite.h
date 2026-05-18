#pragma once

// "Layer" is a heterogenous set of slices with one order and one storage mutability.

#include <unordered_map>

#include <fQSM/meta/axis.h>
#include <fQSM/meta/specializations.h>
#include <fQSM/state/_forwards.h>
#include <fQSM/state/slice.h>

namespace fqsm::state {
    namespace axis = meta::axis;

    template<axis::order ItemRole, axis::mutability SliceStorageType>
    struct Composite {
        struct Entry {
            using Base = slice::Abstract<ItemRole>;
            using Handle = typename meta::state::Reference<SliceStorageType, Base>::Type;

            template<aspect::Any Meta>
            using Typed = slice::Data<Meta, ItemRole>;

            template<aspect::Any Meta>
            using TypedHandle = typename meta::state::Reference<
                SliceStorageType,
                Typed<Meta>
            >::Type;
        };
        using Slices = std::unordered_map<RAId, typename Entry::Handle>;

        Slices slices;
    };
}