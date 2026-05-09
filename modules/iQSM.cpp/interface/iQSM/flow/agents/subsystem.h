#pragma once

#include <functional>
#include <iQSM/_forwards.h>

namespace iqsm::agents {

    struct Subsystem {
        struct Update {
            // Synchronizer sees a participant as:
            // 1) current local snapshot,
            // 2) a way to replace that snapshot after merge/sync.
            Reading current;
            std::function<void(Reading)> replace;
        };
        // TODO: discuss to add Channel interface here
        // this looks useless atm, but...

        virtual ~Subsystem() = default;
        virtual Schema schema() const = 0;
        virtual Update access() = 0;
    };

}