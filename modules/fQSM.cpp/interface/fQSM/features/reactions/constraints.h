#pragma once

#include <format>

#include <fQSM/meta/interface.include.h>
#include <fQSM/features/reaction.h>
#include <fQSM/manipulation/feedback.h>
#include <fQSM/manipulation/item.h>

namespace fqsm::features::reactions::morms::constraints {

    // Local data constraint: ItemChange handler from Aspect::Actions (or its Private).
    template<aspect::Any Meta>
    struct local : Reaction {
        using Corrector = typename Meta::BaseActions::ItemUpdate;

        explicit local(Corrector corrector) : corrector(corrector) {}

        Sources listens() const override { return typed_set<Meta>(); }
        void apply(Reviewing context) override;

    private:
        Corrector corrector = nullptr;
    };
}

// Impl:
namespace fqsm::features::reactions::morms::constraints {

    template<aspect::Any Meta>
    void local<Meta>::apply(Reviewing context) {
        for (const auto change : changes<Meta>(context).addedOrUpdated()) {
            if (!change.after) continue;
            if (!corrector) {
                ask::feedback::critical<Meta>(
                    context,
                    std::format(R"(constraints::local no corrector on "{}" {})", aspect::Rtid::name<Meta>(), change.id));
                continue;
            }
            const auto fix = (*corrector)(*change.after);
            if (!fix) continue;
            *manipulation::item::update<Meta>(context, change.id) = *fix;
        }
    }
}
