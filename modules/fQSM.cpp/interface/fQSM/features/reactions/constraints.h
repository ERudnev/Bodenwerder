#pragma once

#include <format>

#include <fQSM/meta/interface.include.h>
#include <fQSM/features/reaction.h>

namespace fqsm::features::reactions::constraint {

    // Local data constraint: ItemChange handler from Aspect::Actions (or its Private).
    template<category::Any Meta>
    //struct element : Functional<typename Meta::BaseActions::QuantumLocal> {
    //    using Parent = Functional<typename Meta::BaseActions::QuantumLocal>;
    struct element : Functional<typename Meta::BaseActions::Vocabulary::EvaluateQuantumLocal> {
        using Parent = Functional<typename Meta::BaseActions::Vocabulary::EvaluateQuantumLocal>;

        explicit element(Parent::ActionFunction corrector) : Parent(corrector) {}

        Parent::Sources listens() const override { return Abstract::typed_set<Meta>(); }
        void apply(Reacting context) override;
    };

    template<category::Any Meta>
    struct element_wide : Functional<typename Meta::BaseActions::Vocabulary::EvaluateQuantumContextual>
    {
        using Parent = Functional<typename Meta::BaseActions::Vocabulary::EvaluateQuantumContextual>;

        explicit element_wide(Parent::ActionFunction corrector) : Parent(corrector) {}

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
                context.deny(std::format(R"(constraint::item_added_changed no corrector on "{}" {})", Rtid::name<Meta>(), change.id));
                continue;
            }
            const auto fix = this->action(*change.after);
            if (!fix) continue;
            //*Meta::BaseActions::modify(context, change.id) = *fix;
            context.reaction<Meta>().put_modification(change.id, *fix);

        }
    }

    template<category::Any Meta>
    void element_wide<Meta>::apply(Reacting context) {
        for (const auto change : Abstract::changes<Meta>(context).addedOrUpdated()) {
            if (not change.after) {
                context.deny(std::format(R"(constraint::item_added_changed no corrector on "{}" {})", Rtid::name<Meta>(), change.id));
            }
            else {
                const auto fix = this->action(context, change.id, *change.after);
                if (!fix) continue;
                context.reaction<Meta>().put_modification(change.id, *fix);
            }
        }
    }
}
