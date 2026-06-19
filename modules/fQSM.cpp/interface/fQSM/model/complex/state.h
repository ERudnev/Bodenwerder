#pragma once

#include <fQSM/model/_forwards.h>
#include <base/cannonball/table.h>
#include <fQSM/model/structure/composite.h>
#include <fQSM/model/linear/state.h>

namespace fqsm::model::complex {

    struct State {
        using Container = composite::Container<linear::state::Erased>;

        State(Schema schema) : schema(schema) { generateEmptyLines(); }

        const Schema schema;

        // TODO: pus this r/w erased acces into provate part
        template<aspect::Any Meta>
        const linear::State<Meta>& aspect() const;

        template<aspect::Any Meta>
        linear::State<Meta>& aspect() { _INCOMPLETE_; }

    protected:
        using Ref = ref<linear::state::Erased>;
        auto extract(composite::TypeId) -> Ref const;
        void generateEmptyLines();

        Container lines;
    };

    struct StateAddressable : State {
        // own template casting accessors...
    };
}


// impl:
namespace fqsm::model::complex {

    void generateEmptyLines() {
        _INCOMPLETE_;
    }

    auto State::extract(composite::TypeId typeId) -> Ref const {
        _INCOMPLETE_;
        return lines.at(typeId);
    }

    template<aspect::Any Meta>
    const linear::State<Meta>& State::aspect() const {
        _INCOMPLETE_;
    }

}
