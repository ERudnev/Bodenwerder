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

        struct SliceFactory {
            using State = std::function<ref<slice::Abstract<axis::order::state>>()>;
            using StateClone = std::function<ref<slice::Abstract<axis::order::state>>(const world::View&)>;
            using Overlay = std::function<ref<slice::Abstract<axis::order::state>>(const world::View&, const world::Patch&)>;

            State createState;
            StateClone cloneState;
            Overlay createOverlay;
        };

        struct Aspect {
            const meta::aspect::Name name;
            TypeSet requiredByMe;
            TypeSet requiredBy;
            SliceFactory factory;            
        };

        std::unordered_map<meta::aspect::Rtid, Aspect, meta::aspect::Rtid::Hash> aspects;
    };
}