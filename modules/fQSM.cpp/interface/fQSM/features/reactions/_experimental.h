#pragma once

#include <fQSM/meta/interface.include.h>
#include <fQSM/features/reaction.h>

namespace  fqsm::features::reactions::debug {

    template<category::Component Meta>
    struct death_log : Abstract {
        death_log(std::string text) : message(std::move(text)) {}

        const std::string message;

        Sources listens() const override {
            return typed_set<Meta>();
        }

        void apply(Reviewing) override {
            base::message("hey, I am here!");
        }
    };
}
