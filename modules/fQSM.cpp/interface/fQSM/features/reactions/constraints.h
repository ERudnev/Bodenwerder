#pragma once

#include <format>

#include <fQSM/meta/interface.include.h>
#include <fQSM/features/reaction.h>
#include <fQSM/manipulation/feedback.h>
#include <fQSM/manipulation/item.h>

namespace fqsm::features::reactions::constraint {

    // Local data constraint: ItemChange handler from Aspect::Actions (or its Private).
    template<category::Any Meta>
    struct element : Functional<typename Meta::BaseActions::QuantumLocal> {
        using Parent = Functional<typename Meta::BaseActions::QuantumLocal>;

        explicit element(Parent::ActionFunction corrector) : Parent(corrector) {}

        Parent::Sources listens() const override { return Abstract::typed_set<Meta>(); }
        void apply(Reacting context) override;
    };
}

// Impl:
namespace fqsm::features::reactions::constraint {

    template<category::Any Meta>
    void element<Meta>::apply(Reacting context) {
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
