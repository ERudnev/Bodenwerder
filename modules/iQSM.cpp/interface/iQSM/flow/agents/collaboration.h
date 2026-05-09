#pragma once

#include <base/pair.h>

#include <iQSM/schema.h>
#include <iQSM/repository/agents/subsystem.h>

namespace iqsm::agents {
    struct Collaboration {
        Collaboration(ref<Subsystem> left, ref<Subsystem> right);

        base::pair<ref<Subsystem>> peers;

        base::pair<Reading> lastSynced;

        SchemaObject::TypeSet overlapTypes;

        void sync();
    };
}