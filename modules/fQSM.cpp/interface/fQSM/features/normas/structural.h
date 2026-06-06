#pragma once

#include <fQSM/meta/interface.include.h>
#include <fQSM/features/norma.h>

namespace fqsm::processing::normas::structural {
    
    template<aspect::Component ComponentAspect, aspect::Entity EntityAspect>
    struct Component : Norma {

        void apply(const Preview& preUpdate, Patch& fixAccumulator) override {
        }
        
    };
}