#pragma once

#include <fQSM/meta/interface.include.h>
#include <fQSM/features/reaction.h>

namespace  fqsm::features::reactions::standard {

    template<category::Component Meta>
    struct debug_death_event : Abstract {
        debug_death_event(std::string text) : message(std::move(text)) {}

        const std::string message;

        Sources listens() const override {
            return typed_set<Meta>();
        }

        void apply(Reviewing) override {
            base::message("hey, I am here!");
        }
    };
}
