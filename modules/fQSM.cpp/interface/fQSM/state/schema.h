#pragma once

#include <functional>
#include <set>
#include <string>
#include <unordered_map>

#include <fQSM/meta/runtimeId.h>
#include <fQSM/state/_forwards.h>
#include <fQSM/state/slice.h>
#include <fQSM/references.h>
//#include <fQSM/service/validation.h>

namespace fqsm::state {

    namespace axis = meta::axis;
    
    struct SchemaData {
        using TypeSet = std::set<meta::aspect::Rtid>;
        //using Invariants = validation::Block;

        struct Aspect {
            using StateSliceFactory = std::function<ref<slice::Abstract<axis::order::state>>()>;
            using OverlaySliceFactory = std::function<ref<slice::Abstract<axis::order::state>>(const world::View&, const world::Patch&)>;

            const meta::aspect::Name name;
            TypeSet requiredByMe;
            TypeSet requiredBy;
            StateSliceFactory createStateSlice;
            OverlaySliceFactory createOverlaySlice;
        };

        std::unordered_map<meta::aspect::Rtid, Aspect, meta::aspect::Rtid::Hash> aspects;
    };
}