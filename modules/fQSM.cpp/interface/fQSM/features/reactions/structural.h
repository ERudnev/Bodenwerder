#pragma once

#include <format>
#include <fQSM/meta/interface.include.h>
#include <fQSM/features/reaction.h>
#include <fQSM/manipulation/feedback.h>
#include <fQSM/manipulation/item.h>

namespace fqsm::features::reflexes {

    // TODO: find better names
    // TODO: find better place (gather all reflexes in one file?)
    enum class ComponentMissing {
        remove_parent,
        make_default,
        inacceptable,
    };
}

namespace fqsm::features::reactions::morms::structural {

    using fqsm::features::reflexes::ComponentMissing;

    // NG: public visibility of this "classes" is made object-like
    template<aspect::Component Follower, aspect::Entity Origin>
    struct component : Reaction {
        static_assert(std::same_as<typename Follower::HostAspect, Origin>);
        using AutoConstructorType = typename Follower::BaseCapabilities::AutoConstructorType;

        component(ComponentMissing strat, AutoConstructorType autoConstr = nullptr) : policy(strat), autoConstructor(autoConstr) {}

        Sources listens() const override { return typed_set<Origin>(); }
        void apply(Reviewing context) override;
    private:
        const ComponentMissing policy;
        AutoConstructorType* autoConstructor = nullptr;
    };
}

// Impl:
namespace fqsm::features::reactions::morms::structural {
    // component:
    template<aspect::Component Follower, aspect::Entity Origin>
    void component<Follower, Origin>::apply(Reviewing context) {
        // all modes:
        for (const auto change : will_be<Origin>(context).removed()) {
            if (!manipulation::item::exists<Follower>(context.preview, change.id)) continue;
            manipulation::item::update<Follower>(context, change.id).remove();
        }

        switch (policy) {
            case ComponentMissing::remove_parent: {
                for (const auto change : will_be<Origin>(context).added()) {
                    if (manipulation::item::exists<Follower>(context.preview, change.id)) continue;
                    manipulation::item::update<Origin>(context, change.id).remove();
                }
            } break;

            case ComponentMissing::make_default: {
                for (const auto change : will_be<Origin>(context).added()) {
                    if (manipulation::item::exists<Follower>(context.preview, change.id)) continue;
                    if (!autoConstructor) {
                        ask::feedback::critical<Follower>(context, std::format(R"(structural::component no constructor on "{}" {})", aspect::Rtid::name<Origin>(), change.id));
                        continue;
                    }
                    (*autoConstructor)(context, change.id);
                }
            } break;

            case ComponentMissing::inacceptable: {
                for (const auto change : will_be<Origin>(context).added()) {
                    if (manipulation::item::exists<Follower>(context.preview, change.id)) continue;
                    ask::feedback::critical<Follower>(context, std::format(R"(structural::component inacceptable: missing for "{}" {})", aspect::Rtid::name<Origin>(), change.id));
                }
            } break;
        }
    }
}
