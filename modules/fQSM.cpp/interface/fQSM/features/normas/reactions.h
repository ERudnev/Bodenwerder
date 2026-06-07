#pragma once

#include <fQSM/meta/interface.include.h>
#include <fQSM/features/norma.h>

namespace fqsm::features::normas::structural {

    template<aspect::Component Aspect>
    struct debug_death_event : Norma {

        debug_death_event(std::string text) : message(std::move(text)) {}

        inline static const Filter filter{
            state::item::ChangeType::deletion,
        };

        const std::string message;

        void apply(Reviewing) override {
            base::message("");
        }
    };
}
