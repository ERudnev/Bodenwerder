#pragma once

#include <fQSM/meta/interface.include.h>
#include <fQSM/features/reaction.h>
#include <fQSM/manipulation/item.h>

namespace fqsm::features::reactions::rules::structural {

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
namespace fqsm::features::reactions::rules::structural {

    // remove_with_parent
    template<category::Parasitic Parasitic, category::Any Parent>
    struct remove_with_parent : Abstract {
        Abstract::Sources listens() const override { return Abstract::typed_set<Parent>(); }
        void apply(Reviewing context) override {
            auto& target = context.reaction<Parasitic>();
            for (const auto& change : Abstract::changes<Parent>(context).removed())
                target.put_deletion(change.id);
        }
    };

    // dead_component_kill_parent
    template<category::Component Parasitic, category::Any Parent>
    struct dead_component_kill_parent : Abstract {
        Abstract::Sources listens() const override { return Abstract::typed_set<Parasitic>(); }
        void apply(Reviewing context) override {
            auto& target = context.reaction<Parent>();
            for (const auto& change : Abstract::changes<Parasitic>(context).removed())
                target.put_deletion(change.id);
        }
    };

    // parastic_requires_parent_to_appear
    template<category::Parasitic Parasitic, category::Any Parent>
    struct parastic_requires_parent_to_appear : Abstract {
        Abstract::Sources listens() const override { return Abstract::typed_set<Parasitic>(); }
        void apply(Reviewing context) override {
            auto& target = context.reaction<Parasitic>();
            const auto& source = context.proposal.aspect<Parent>();
            for (const auto& change : Abstract::changes<Parasitic>(context).added()) {
                if (source.items().find(change.id))
                    continue;
                target.put_deletion(change.id);
            }
        }
    };


    // component_default_construction_if_defined
    template<category::Component Parasitic, category::Any Parent>
    struct parrent_appears_requires_component : Abstract {
        Abstract::Sources listens() const override { return Abstract::typed_set<Parent>(); }
        void apply(Reviewing context) override {
            // component class has constraint: component item must be added manually in the same patch as parent
            // this is strict, nut effective against "lost" parts of Archetype init and default c-tors (with hard-to-catch zeroes)
            // and if component is forgotten, kill PARENT!
            // yes, so rude but very strict policy
            auto& target = context.reaction<Parent>();
            const auto& source = context.proposal.aspect<Parasitic>();
            for (const auto& change : Abstract::changes<Parent>(context).added()) {
                if (source.items().find(change.id))
                    continue;
                target.put_deletion(change.id);
            }

        }
    };
}