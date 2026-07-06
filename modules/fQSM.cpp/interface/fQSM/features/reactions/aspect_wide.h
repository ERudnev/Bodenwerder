#pragma once

#include <stdexcept>

#include <fQSM/meta/interface.include.h>
#include <fQSM/features/reaction.h>

namespace fqsm::features::reactions {

    template<category::Any Owner, category::Any... ExtraSources>
    struct aspect_wide : Abstract {
        using Handler = typename Owner::DefaultInternals::TemporaryFreeReaction;

        explicit aspect_wide(Handler handler) : handler(handler) {}

        Sources listens() const override {
            return Abstract::typed_set<Owner, ExtraSources...>();
        }

        void apply(Reacting context) override {
            if (!handler) throw std::runtime_error("null func");
            handler(context);
        }

    private:
        Handler handler = nullptr;
    };
}
