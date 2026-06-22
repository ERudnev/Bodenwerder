#pragma once

#include <format>

#include <fQSM/meta/interface.include.h>
#include <fQSM/features/reaction.h>
#include <fQSM/manipulation/feedback.h>
#include <fQSM/manipulation/item.h>

namespace fqsm::features::reactions::rules::constraint {

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
}

// Impl:
namespace fqsm::features::reactions::rules::constraint {

    template<category::Any Meta>
    void value_X<Meta>::apply(Reviewing context) {
        for (const auto change : Abstract::changes<Meta>(context).addedOrUpdated()) {
            if (not change.after) {
                // TOFO: wrap block under "if"
                ask::feedback::critical(
                    context,
                    std::format(R"(constraint::item_added_changed no corrector on "{}" {})", Rtid::name<Meta>(), change.id));
            }
            else {
                const auto fix = this->action(*change.after);
                if (!fix) continue;
                *manipulation::item::update<Meta>(context, change.id) = *fix;
            }
        }
    }
}
