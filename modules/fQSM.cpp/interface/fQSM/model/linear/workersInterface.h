#pragma once

#include <fQSM/meta/interface.include.h>

namespace fqsm::model::linear {

    template<category::Any Meta>
    struct WorkersInterface { // Zag Zag!
        virtual ~WorkersInterface()=default;

        virtual void put_modification(Id<Meta>, Quantum<Meta>) = 0;
        virtual void put_deletion(Id<Meta>) = 0;
        virtual void put_add(Id<Meta>, Quantum<Meta>) = 0;
        virtual void put_global(GlobalValue<Meta>) = 0;
        virtual Quantum<Meta>& get_modification_access(Id<Meta>)=0;
        // old ver: virtual Quantum<Meta>& update_modification(Id<Meta>, base::function_ref<const Quantum<Meta>&()> prepatch) = 0;
        virtual GlobalValue<Meta>& get_access_global()=0;
        // old ver; virtual GlobalValue<Meta>& update_global(base::function_ref<const GlobalValue<Meta>&()> prepatch) = 0;
    };

}