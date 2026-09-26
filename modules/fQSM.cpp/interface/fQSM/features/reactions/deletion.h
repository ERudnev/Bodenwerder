#pragma once

#include <string>

#include <fQSM/meta/interface.include.h>
#include <fQSM/features/reaction.h>

namespace fqsm::features::reactions {

    // Calls fn(Retrospecting, id, last value) once per removed item. Retrospecting reads the last stable state.
    template<category::Any Meta>
    struct deletion : Functional<typename Meta::BaseActions::Vocabulary::JustRetrospecting> {
        using Parent = Functional<typename Meta::BaseActions::Vocabulary::JustRetrospecting>;

        explicit deletion(Parent::ActionFunction reaction) : Parent(reaction) {}
        Parent::Sources listens() const override { return Abstract::typed_set<Meta>(); }
        void apply(Reacting context) override {
            for (const auto change : Abstract::changes<Meta>(context).removed())
                this->action(context.retrospecting(), change.id, change.old);
        }
    };
}

namespace fqsm::features::reactions::debug {

    // Adds a warning (not a refusal) to every wave that changes Meta.
    template<category::Any Meta>
    struct death_log : Abstract {
        explicit death_log(std::string text) : message(std::move(text)) {}

        const std::string message;

        Sources listens() const override { return typed_set<Meta>(); }
        void apply(Reacting context) override { context.warning("experimental death detected"); }
    };
}
