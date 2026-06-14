#pragma once

#include <fQSM/meta/interface.include.h>
#include <fQSM/state/aspect/erased.h>
#include <fQSM/state/aspect/storage.h>

namespace fqsm::state::slice {

    /*
    template<aspect::Any Meta>
    struct Draft : Interface<Meta>, Changes<Meta> {
        // slice::Interface:
        ContainerAbstract& items() override;
        Global& global() override;

        // Draft interface:


    protected:
        cref<Actual<Meta>> actual;
        cref<Patch<Meta>> patch;
    };
    */

    template<aspect::Any Meta>
    struct Draft :

}