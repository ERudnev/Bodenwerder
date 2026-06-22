#pragma once

#include <format>

#include <fQSM/meta/interface.include.h>
#include <fQSM/features/reaction.h>
#include <fQSM/manipulation/feedback.h>
#include <fQSM/manipulation/item.h>

namespace fqsm::features::reactions::rules::constraints {

    // Local data constraint: ItemChange handler from Aspect::Actions (or its Private).
    template<category::Any Meta>
    struct value_X : Functional<typename Meta::BaseActions::QuantumLocal> {
        using Parent = Functional<typename Meta::BaseActions::QuantumLocal>;

        explicit value_X(Parent::ActionFunction corrector) : Parent(corrector) {}

        Parent::Sources listens() const override { return Abstract::typed_set<Meta>(); }
        void apply(Reviewing context) override;
    };

    template<category::Any Meta>
    struct quantum_dependent : Functional<typename Meta::BaseActions::UpdateValue> {
        // TODO: generic version of value_X (sees whole world to get access to dependencies
    };

    template<category::Any Meta>
    struct item_destroyed : Functional<typename Meta::BaseActions::Action> {
        using Parent = Functional<typename Meta::BaseActions::Action>;

        explicit item_destroyed(Parent::ActionFunction reaction) : Parent(reaction) {}
        Parent::Sources listens() const override { return Abstract::typed_set<Meta>(); }
        void apply(Reviewing context) override;
    };
}

// Impl:
namespace fqsm::features::reactions::rules::constraints {

    template<category::Any Meta>
    void value_X<Meta>::apply(Reviewing context) {
        for (const auto change : Abstract::changes<Meta>(context).addedOrUpdated()) {
            if (not change.after) {
                // TOFO: wrap block under "if"
                ask::feedback::critical(
                    context,
                    std::format(R"(constraints::item_added_changed no corrector on "{}" {})", Rtid::name<Meta>(), change.id));
            }
            else {
                const auto fix = this->action(*change.after);
                if (!fix) continue;
                *manipulation::item::update<Meta>(context, change.id) = *fix;
            }
        }
    }

    template<category::Any Meta>
    void item_destroyed<Meta>::apply(Reviewing context) {
        for (const auto change : Abstract::changes<Meta>(context).removed()) {
            if (not change.before)
                ask::feedback::critical(context, std::format(R"(constraints::item_destroyed failed on "{}" {})", Rtid::name<Meta>(), change.id));
            else
                this->action(context, change.id, change.throwing_before());
                //if (!fix) continue;
                //*manipulation::item::update<Meta>(context, change.id) = *fix;
        }
    }
}
