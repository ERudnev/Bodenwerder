#pragma once

#include <format>

#include <fQSM/meta/interface.include.h>
#include <fQSM/features/reaction.h>
#include <fQSM/processing/_forwards.h>

namespace fqsm::features::reactions::structural {

    template<category::Parasitic Parasitic, category::Any Parent>
    struct remove_with_parent;

    template<category::Parasitic Parasitic, category::Any Parent>
    struct dead_parasitic_kill_parent;

    template<category::Parasitic Parasitic, category::Any Parent>
    struct new_parasitic_requires_existing_parent;

    template<category::Parasitic Parasitic, category::Any Parent>
    struct new_parasitic_requires_parent_appears;

    template<category::Component Parasitic, category::Any Parent>
    struct parent_appears_requires_component;

    template<category::Group GroupMeta, category::Any Element>
    struct group_removal_removes_elements;

}

// Impl:
namespace fqsm::features::reactions::structural {

    // remove_with_parent
    template<category::Parasitic Parasitic, category::Any Parent>
    struct remove_with_parent : Abstract {
        Abstract::Sources listens() const override { return Abstract::typed_set<Parent>(); }
        void apply(Reacting context) override {
            auto& target = context.reaction<Parasitic>();
            for (const auto& change : Abstract::changes<Parent>(context).removed()) {
                _DBG_TX_("structural remove_with_parent: {} removed -> put_deletion {} {}", Rtid::name<Parent>(), Rtid::name<Parasitic>(), change.id);
                target.put_deletion(change.id);
            }
        }
    };

    // dead_parasitic_kill_parent
    template<category::Parasitic Parasitic, category::Any Parent>
    struct dead_parasitic_kill_parent : Abstract {
        Abstract::Sources listens() const override { return Abstract::typed_set<Parasitic>(); }
        void apply(Reacting context) override {
            auto& target = context.reaction<Parent>();
            for (const auto& change : Abstract::changes<Parasitic>(context).removed()) {
                _DBG_TX_("structural dead_parasitic_kill_parent: {} removed -> put_deletion {} {}", Rtid::name<Parasitic>(), Rtid::name<Parent>(), change.id);
                target.put_deletion(change.id);
            }
        }
    };

    // new_parasitic_requires_existing_parent
    template<category::Parasitic Parasitic, category::Any Parent>
    struct new_parasitic_requires_existing_parent : Abstract {
        Abstract::Sources listens() const override { return Abstract::typed_set<Parasitic>(); }
        void apply(Reacting context) override {
            const auto& source = context.proposal.aspect<Parent>();
            for (const auto& change : Abstract::changes<Parasitic>(context).added()) {
                if (source.items().find(change.id)) {
                    _DBG_TX_("structural new_parasitic_requires_existing_parent: {} {} ok, {} in proposal", Rtid::name<Parasitic>(), change.id, Rtid::name<Parent>());
                    continue;
                }
                _DBG_TX_("structural new_parasitic_requires_existing_parent: CRITICAL {} {}, {} missing in proposal", Rtid::name<Parasitic>(), change.id, Rtid::name<Parent>());
                context.refuse(std::format(R"(structural: {} missing for new {} {})", Rtid::name<Parent>(), Rtid::name<Parasitic>(), change.id));
            }
        }
    };

    // new_parasitic_requires_parent_appears
    template<category::Parasitic Parasitic, category::Any Parent>
    struct new_parasitic_requires_parent_appears : Abstract {
        Abstract::Sources listens() const override { return Abstract::typed_set<Parasitic>(); }
        void apply(Reacting context) override {
            const auto parentDelta = Abstract::changes<Parent>(context);
            for (const auto& change : Abstract::changes<Parasitic>(context).added()) {
                bool parent_appears_in_same_patch = false;
                for (const auto& parentChange : parentDelta.added()) {
                    if (parentChange.id == change.id) {
                        parent_appears_in_same_patch = true;
                        break;
                    }
                }
                if (parent_appears_in_same_patch) {
                    _DBG_TX_("structural new_parasitic_requires_parent_appears: {} {} ok, {} added in same patch", Rtid::name<Parasitic>(), change.id, Rtid::name<Parent>());
                    continue;
                }
                _DBG_TX_("structural new_parasitic_requires_parent_appears: CRITICAL {} {}, {} not added in same patch", Rtid::name<Parasitic>(), change.id, Rtid::name<Parent>());
                context.refuse(std::format(R"(structural: {} must appear in the same patch as new {} {})", Rtid::name<Parent>(), Rtid::name<Parasitic>(), change.id));
            }
        }
    };


    template<category::Component Parasitic, category::Any Parent>
    struct parent_appears_requires_component : Abstract {
        Abstract::Sources listens() const override { return Abstract::typed_set<Parent>(); }
        void apply(Reacting context) override {
            const auto& source = context.proposal.aspect<Parasitic>();
            for (const auto& change : Abstract::changes<Parent>(context).added()) {
                if (source.items().find(change.id)) {
                    _DBG_TX_("structural parent_appears_requires_component: {} {} ok, {} in proposal", Rtid::name<Parent>(), change.id, Rtid::name<Parasitic>());
                    continue;
                }
                _DBG_TX_("structural parent_appears_requires_component: CRITICAL {} {}, {} missing in proposal", Rtid::name<Parent>(), change.id, Rtid::name<Parasitic>());
                context.refuse(std::format(R"(structural: {} missing for new {} {})", Rtid::name<Parasitic>(), Rtid::name<Parent>(), change.id));
            }
        }
    };

    template<category::Group GroupMeta, category::Any Element>
    struct group_removal_removes_elements : Abstract {
        Abstract::Sources listens() const override { return Abstract::typed_set<GroupMeta>(); }
        void apply(Reacting context) override {
            auto& target = context.reaction<Element>();
            for (const auto& change : Abstract::changes<GroupMeta>(context).removed()) {
                for (const auto& id : change.throwing_before()) {
                    _DBG_TX_("structural group_removal: {} removed -> put_deletion {} {}", Rtid::name<GroupMeta>(), Rtid::name<Element>(), id);
                    target.put_deletion(id);
                }
            }
        }
    };
}