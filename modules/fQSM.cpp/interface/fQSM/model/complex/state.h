#pragma once

#include <fQSM/model/_forwards.h>
#include <base/cannonball/table.h>
#include <fQSM/model/structure/composite.h>
#include <fQSM/model/linear/state.h>

namespace fqsm::model::complex {

    class State {
    public:
        //using Container = composite::Container<linear::state::Erased>;
        using Composition = Composite<linear::state::Erased>;

        State(Schema schema) : schema(schema) {}
        virtual ~State()=default;

        template<aspect::Any Meta>
        const linear::State<Meta>& aspect() const {
            return base::shared_ref_cast<linear::State<Meta>>(composition().container.at(Rtid::of<Meta>));
        }

        template<aspect::Any Meta>
        linear::State<Meta>& aspect() {
            return base::shared_ref_cast<linear::State<Meta>>(composition().container.at(Rtid::of<Meta>));
        }

        const Schema schema; // defined for Reality/Draft/any homogenous material object
    protected:
        using Erased = linear::state::Erased;
        virtual cref<Erased> getLine(meta::aspect::Rtid) const = 0;
        virtual ref<Erased> getLine(meta::aspect::Rtid) = 0;
        virtual const Composition& composition() const = 0;
    };

    struct StateAddressable : State {
        // own template casting accessors...
    };
}
