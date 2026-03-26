#pragma once

#include <functional>
#include <utility>

#include <iQSM/_forwards.h>

namespace iqsm::repo {

    // Value-type commit handle:
    // - copyable/movable
    // - no virtual/polymorphism
    // - delegates `push()` into captured receiver (e.g. Branch)
    struct Commit final {
        World initial;
        std::function<void(Delta)> receiver;

        Commit(World initial, std::function<void(Delta)> receiver)
            : initial(std::move(initial))
            , receiver(std::move(receiver))
        {}

        void push(Delta delta) const {
            receiver(std::move(delta));
        }
    };
}