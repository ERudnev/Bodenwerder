#pragma once

#include <fQSM/meta/interface.include.h>
#include <fQSM/features/reaction.h>
#include <fQSM/manipulation/feedback.h>

namespace fqsm::features::reactions {

    // Local data constraint: ItemChange handler from Aspect::Actions (or its Private).
    template<category::Any Meta>
    struct binding : Functional<typename Meta::BaseActions::Action> {
        using Parent = Functional<typename Meta::BaseActions::Action>;

        explicit binding(Parent::ActionFunction corrector) : Parent(corrector) {}

        // declare binding to events of type Meta>
        Parent::Sources listens() const override {
            return Abstract::typed_set<Meta>();
        }
        void apply(Reacting context) override;
    };
}