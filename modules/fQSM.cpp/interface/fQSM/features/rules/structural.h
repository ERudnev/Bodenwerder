#pragma once

#include <fQSM/meta/interface.include.h>
#include <fQSM/features/reaction.h>

namespace fqsm::features::reactions::rules::structural {

    template<category::Component Follower, category::Any Parent>
    struct remove_with_parent;


}

// Impl:
namespace fqsm::features::reactions::rules::structural {

    template<category::Component Follower, category::Any Parent>
    struct remove_with_parent : Abstract {

        Abstract::Sources listens() const override { return Abstract::typed_set<Parent>(); }
        void apply(Reviewing context) override {
            auto& target = context.reaction<Follower>();
            for (const auto& change : Abstract::changes<Parent>(context).removed())
                target.put_deletion(change.id);
        }
    };
}