#pragma once

#include <fQSM/state/world.h>
#include <fQSM/processing/context.h>

namespace fqsm::processing {

    struct Realm : Context {
        Realm(const state::world::View& initial) : world(initial) {}

    private:
        state::world::Data world;
    };
}