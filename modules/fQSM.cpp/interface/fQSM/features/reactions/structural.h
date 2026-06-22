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
    template<category::Component Follower, category::Any Origin>
    struct component : Functional<typename Follower::BaseActions::ConstructFromParent> {
        using Parent = Functional<typename Follower::BaseActions::ConstructFromParent>;

        static_assert(std::same_as<typename Follower::HostAspect, Origin>);
        component(ComponentMissing strat, Parent::ActionFunction autoConstructor = nullptr) : Parent(autoConstructor), policy(strat) {}

        Parent::Sources listens() const override { return Abstract::typed_set<Origin>(); }
        void apply(Reviewing context) override;
    private:
        const ComponentMissing policy;
    };
}

// Impl:
namespace fqsm::features::reactions::morms::structural {
    // component:
    template<category::Component Follower, category::Any Origin>
    void component<Follower, Origin>::apply(Reviewing context) {
        // all modes:
        for (const auto change : Abstract::changes<Origin>(context).removed()) {
            if (!manipulation::item::exists<Follower>(context.proposal, change.id)) continue;
            manipulation::item::update<Follower>(context, change.id).remove();
        }

        switch (policy) {
            case ComponentMissing::remove_parent: {
                for (const auto change : Abstract::changes<Origin>(context).added()) {
                    if (manipulation::item::exists<Follower>(context.proposal, change.id)) continue;
                    manipulation::item::update<Origin>(context, change.id).remove();
                }
            } break;

            case ComponentMissing::make_default: {
                for (const auto change : Abstract::changes<Origin>(context).added()) {
                    if (manipulation::item::exists<Follower>(context.proposal, change.id))
                        continue;
                    static const std::string prepared_message = std::format("required default c-tor: {}", Rtid::name<Follower>());
                    if (not this->optionally_callable(context, prepared_message))
                        continue;
                    this->action(context, change.id);
                }
            } break;

            case ComponentMissing::inacceptable: {
                for (const auto change : Abstract::changes<Origin>(context).added()) {
                    if (manipulation::item::exists<Follower>(context.proposal, change.id)) continue;
                    ask::feedback::critical(context, std::format(R"({}: structural::component inacceptable: missing for "{}" {})", Rtid::name<Follower>(), Rtid::name<Origin>(), change.id));
                }
            } break;
        }
    }
}
