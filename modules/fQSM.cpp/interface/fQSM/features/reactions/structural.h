#pragma once

#include <fQSM/meta/interface.include.h>
#include <fQSM/features/reaction.h>
#include <fQSM/manipulation/item.h>

namespace fqsm::features::reactions::normas::structural {

    // NG: public visibility of this "classes" is made object-like
    template<aspect::Component Follower, aspect::Entity Origin>
    struct component : Reaction {
        static_assert(std::same_as<typename Follower::HostAspect, Origin>);

        enum class Behavior {
            removeParent,
            addDefault,
            verifyAdded,
        };

        component(Behavior strat = Behavior::verifyAdded) : behavior(strat) {}

        Sources listens() const override {
            return typed_set<Origin>();
        }

        void apply(Reviewing context) override {
            switch (behavior) {
                case Behavior::removeParent: {
                    for (const auto change : will_be<Origin>(context).added()) {
                        if (manipulation::item::exists<Follower>(context.preview, change.id)) continue;
                        manipulation::item::update<Origin>(context, change.id).remove();
                    }

                    for (const auto change : will_be<Origin>(context).removed()) {
                        if (!manipulation::item::exists<Follower>(context.preview, change.id)) continue;
                        manipulation::item::update<Follower>(context, change.id).remove();
                    }
                } break;

                case Behavior::addDefault: {
                } break;

                case Behavior::verifyAdded: {
                } break;
            }
        }
    private:
        const Behavior behavior;
    };
}
