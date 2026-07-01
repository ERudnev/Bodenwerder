#pragma once

#include <format>

#include <fQSM/meta/interface.include.h>
#include <fQSM/features/reaction.h>
#include <fQSM/manipulation/item.h>
#include <fQSM/manipulation/feedback.h>

namespace fqsm::features::reactions::structural {

    template<category::Parasitic Parasitic, category::Any Parent>
    struct remove_with_parent;

    template<category::Component Parasitic, category::Any Parent>
    struct dead_component_kill_parent;

    template<category::Parasitic Parasitic, category::Any Parent>
    struct parastic_requires_parent_to_appear;

    template<category::Component Parasitic, category::Any Parent>
    struct parrent_appears_requires_component;

}

// Impl:
namespace fqsm::features::reactions::structural {

    // remove_with_parent
    template<category::Parasitic Parasitic, category::Any Parent>
    struct remove_with_parent : Abstract {
        Abstract::Sources listens() const override { return Abstract::typed_set<Parent>(); }
        void apply(Reacting context) override {
            auto& target = context.reaction<Parasitic>();
            for (const auto& change : Abstract::changes<Parent>(context).removed())
                target.put_deletion(change.id);
        }
    };

    // dead_component_kill_parent
    template<category::Component Parasitic, category::Any Parent>
    struct dead_component_kill_parent : Abstract {
        Abstract::Sources listens() const override { return Abstract::typed_set<Parasitic>(); }
        void apply(Reacting context) override {
            auto& target = context.reaction<Parent>();
            for (const auto& change : Abstract::changes<Parasitic>(context).removed())
                target.put_deletion(change.id);
        }
    };

    // parastic_requires_parent_to_appear
    template<category::Parasitic Parasitic, category::Any Parent>
    struct parastic_requires_parent_to_appear : Abstract {
        Abstract::Sources listens() const override { return Abstract::typed_set<Parasitic>(); }
        void apply(Reacting context) override {
            auto& target = context.reaction<Parasitic>();
            const auto& source = context.proposal.aspect<Parent>();
            for (const auto& change : Abstract::changes<Parasitic>(context).added()) {
                if (source.items().find(change.id))
                    continue;
                target.put_deletion(change.id);
            }
        }
    };


    // parrent_appears_requires_component
    template<category::Component Parasitic, category::Any Parent>
    struct parrent_appears_requires_component : Abstract {
        Abstract::Sources listens() const override { return Abstract::typed_set<Parent>(); }
        void apply(Reacting context) override {
            const auto& source = context.proposal.aspect<Parasitic>();
            for (const auto& change : Abstract::changes<Parent>(context).added()) {
                if (source.items().find(change.id))
                    continue;
                ask::feedback::critical(
                    context,
                    std::format(
                        R"(structural: {} missing for new {} {})",
                        Rtid::name<Parasitic>(),
                        Rtid::name<Parent>(),
                        change.id));
            }
        }
    };
}