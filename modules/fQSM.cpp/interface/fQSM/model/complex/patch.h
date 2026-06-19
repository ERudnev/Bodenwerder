#pragma once

#include <fQSM/meta/interface.include.h>
#include <fQSM/model/_forwards.h>
#include <fQSM/model/linear/patch.h>
#include <fQSM/model/structure/composite.h>

namespace fqsm::model::complex {

    struct Patch {
        const Schema schema;
        const Composite<linear::patch::Erased> lines;

        Patch(Schema schema) : schema(schema) { initStructure(); }

        template<aspect::Any Meta>
        linear::Patch<Meta>& aspect();

        template<aspect::Any Meta>
        const linear::Patch<Meta>& aspect() const;

        linear::patch::Erased& aspect(meta::aspect::Rtid);
        const linear::patch::Erased& aspect(meta::aspect::Rtid) const;

    private:
        void initStructure() { _INCOMPLETE_; }
    };
}

namespace fqsm::model::complex {

    template<aspect::Any Meta>
    linear::Patch<Meta>& Patch::aspect() {
        return base::shared_ref_cast<linear::Patch<Meta>>(lines.container.at(Rtid::of<Meta>));
    };

    template<aspect::Any Meta>
    const linear::Patch<Meta>& Patch::aspect() const {
        return base::shared_ref_cast<linear::Patch<Meta>>(lines.container.at(Rtid::of<Meta>));
    }
}
