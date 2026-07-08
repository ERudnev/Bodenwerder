#pragma once

#include <fQSM/model/_forwards.h>
#include <fQSM/model/intertype/schema.h>

namespace fqsm::processing::orchestrator {

    // unlike Realm/Branch in it not a complete class...
    class Subsystem {
    public:
        virtual ~Subsystem() = default;

        virtual Schema interfaceSchema() const = 0;
    };
}