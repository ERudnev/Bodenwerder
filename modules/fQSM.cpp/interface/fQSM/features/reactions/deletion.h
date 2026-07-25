#pragma once

#include <fQSM/meta/interface.include.h>
#include <fQSM/features/reaction.h>

namespace fqsm::features::reactions {

    template<category::Any Meta>
    struct deletion : Functional<typename Meta::BaseActions::Vocabulary::JustRetrospecting> {
        using Parent = Functional<typename Meta::BaseActions::Vocabulary::JustRetrospecting>;

        explicit deletion(Parent::ActionFunction reaction) : Parent(reaction) {}
        Parent::Sources listens() const override { return Abstract::typed_set<Meta>(); }
        void apply(Reacting context) override;
    };
}

// Impl:
namespace fqsm::features::reactions {

    template<category::Any Meta>
    void deletion<Meta>::apply(Reacting context) {
        for (const auto change : Abstract::changes<Meta>(context).removed()) {
            this->action(::fqsm::Retrospecting{context.retrospective}, change.id, change.old);
        }
    }
}
