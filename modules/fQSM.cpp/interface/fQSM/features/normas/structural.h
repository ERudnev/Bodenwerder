#pragma once

#include <fQSM/meta/interface.include.h>
#include <fQSM/features/norma.h>

namespace fqsm::features::normas::structural {
    
    // NG: public visibility of this "classes" is made object-like
    template<aspect::Component Follower, aspect::Entity Origin>
    struct component : Norma {
        Sources listens() const override {
            return typed_set<Origin>();
        }

        void apply(Reviewing context) override {
            //_INCOMPLETE_;
            if (true)
            {
                //const Id<ComponentAspect> temp_id;
                //manipulators::item::remove<ComponentAspect>(myContext, temp_id);
            }
        }

    };
}
