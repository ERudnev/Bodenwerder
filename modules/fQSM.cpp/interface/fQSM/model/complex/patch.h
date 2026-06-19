#pragma once

#include <fQSM/meta/interface.include.h>
#include <fQSM/model/_forwards.h>
#include <fQSM/model/linear/patch.h>

namespace fqsm::model::complex {

    struct Patch {
        const Schema schema;

        template<aspect::Any Meta>
        const linear::Patch<Meta>& aspect() const;
    };
}

namespace fqsm::model::complex {

    template<aspect::Any Meta>
    const linear::Patch<Meta>& Patch::aspect() const {
        _INCOMPLETE_;
    }

}
