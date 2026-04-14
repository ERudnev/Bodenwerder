#pragma once

#include <base/pair.h>

#include <iQSM/schema.h>
#include <iQSM/world.h>
#include <iQSM/repository/agents/subsystem.h>

namespace iqsm::agents {
    struct Collaboration {
        Collaboration(ref<Subsystem> left, ref<Subsystem> right);

        base::pair<ref<Subsystem>> peers;

        // Последние World слева и справа после успешного sync (per-side snapshot).
        base::pair<World> lastSynced;

        // Множество TypeId общей поверхности (пересечение Schema левого и правого Subsystem).
        SchemaObject::TypeSet overlapTypes;

        void sync();
    };
}