#pragma once

#include <format>

#include <fQSM/meta/interface.include.h>
#include <fQSM/features/reaction.h>
#include <fQSM/manipulation/feedback.h>
#include <fQSM/manipulation/item.h>

namespace fqsm::features::reactions {

    template<category::Any Meta>
    struct deletion : Functional<typename Meta::BaseActions::Action> {
        using Parent = Functional<typename Meta::BaseActions::Action>;

        explicit deletion(Parent::ActionFunction reaction) : Parent(reaction) {}
        Parent::Sources listens() const override { return Abstract::typed_set<Meta>(); }
        void apply(Reviewing context) override;
    };
}

// Impl:
namespace fqsm::features::reactions {

    template<category::Any Meta>
    void deletion<Meta>::apply(Reviewing context) {
        for (const auto change : Abstract::changes<Meta>(context).removed()) {
            if (not change.before)
                ask::feedback::critical(context, std::format(R"(reaction::deletion failed on "{}" {})", Rtid::name<Meta>(), change.id));
            else
                this->action(context, change.id, change.throwing_before());
                //if (!fix) continue;
                //*manipulation::item::update<Meta>(context, change.id) = *fix;
        }
    }
}
