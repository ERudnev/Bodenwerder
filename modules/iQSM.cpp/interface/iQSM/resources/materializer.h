#pragma once

#include <iQSM/_forwards.h>
#include <iQSM/references.h>

namespace iqsm::resources {
    struct ManagerCore;

    template<typename Meta>
    struct Materializer {
        using Manager = ref<ManagerCore>;
        using Id = typename Meta::Id;
        using RuntimeStorage = typename Meta::RuntimeStorage;
        using RuntimeAccess = typename Meta::RuntimeAccess;

        virtual ~Materializer() = default;

        virtual void materialize(Manager, World, Id) const = 0;
        virtual void release(Manager, World, Id) const = 0;
    };
}
