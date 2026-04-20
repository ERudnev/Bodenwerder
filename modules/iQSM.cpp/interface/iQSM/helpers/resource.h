#pragma once

#include <utility>

#include <base/shared_reference.h>

#include <iQSM/helpers/particle.h>
#include <iQSM/internals/delta_builders.h>
#include <iQSM/meta/concepts.h>
#include <iQSM/meta/facade.h>
#include <iQSM/repository/transactions/once.h>
#include <iQSM/resources/manager.h>

namespace iqsm::helpers::resource {
    using Manager = ref<::iqsm::resources::ManagerCore>;
    using Provider = cref<::iqsm::resources::ManagerCore>;

    template<meta::Handle Meta>
    auto declare(Writing, Quantum<Meta>) -> Id<Meta>;

    template<meta::Handle Meta>
    auto create(Writing, Manager, Quantum<Meta>, RuntimeStorage<Meta>) -> Id<Meta>;

    template<meta::Handle Meta>
    auto materialized(Provider, const Id<Meta>&) -> bool;

    template<meta::Handle Meta>
    void materialize(Reading, Manager, const Id<Meta>&);

    template<meta::Handle Meta>
    void release(Reading, Manager, const Id<Meta>&);

    template<meta::Handle Meta>
    auto provide(Reading, const Id<Meta>&) -> RuntimeAccess<Meta>;
}

namespace iqsm::helpers::resource {
    template<meta::Handle Meta>
    auto declare(Writing writing, Quantum<Meta> quantum) -> Id<Meta> {
        const auto id = Id<Meta>::generate_random();
        repo::Once(writing).submit(internals::delta::make_atomic<Meta>(
            id,
            std::nullopt,
            base::make_shared<const Quantum<Meta>>(std::move(quantum))));
        return id;
    }

    template<meta::Handle Meta>
    auto create(
        Writing writing,
        Manager manager,
        Quantum<Meta> quantum,
        RuntimeStorage<Meta> runtime) -> Id<Meta>
    {
        const auto id = declare<Meta>(writing, std::move(quantum));
        manager->layer<Meta>().materialize(id, std::move(runtime));
        return id;
    }

    template<meta::Handle Meta>
    auto materialized(Provider provider, const Id<Meta>& id) -> bool {
        return provider->materialized<Meta>(id);
    }

    template<meta::Handle Meta>
    void materialize(Reading world, Manager manager, const Id<Meta>& id) {
        manager->materializer<Meta>().materialize(manager, world, id);
    }

    template<meta::Handle Meta>
    void release(Reading world, Manager manager, const Id<Meta>& id) {
        manager->materializer<Meta>().release(manager, world, id);
    }

    template<meta::Handle Meta>
    auto provide(Reading world, const Id<Meta>& id) -> RuntimeAccess<Meta> {
        return world->resources->layer<Meta>().provide(id);
    }
}