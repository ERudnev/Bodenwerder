#pragma once

#include <memory>

#include <iQSM/binding/layer.h>
#include <iQSM/binding/resource.h>
#include <iQSM/helpers/binding.h>
#include <iQSM/helpers/particle.h>
#include <iQSM/meta/concepts.h>
#include <iQSM/repository/commit.h>

namespace iqsm::helpers::resource {
    // sugar:
    using Provider = cref<::iqsm::binding::ManagerData>;
    using Manager = ref<::iqsm::binding::ManagerData>;

    template<meta::Binding Meta>
    auto declare(repo::Commit, Quantum<Meta>) -> Id<Meta>;

    template<meta::Binding Meta>
    auto create(repo::Commit, Manager, Quantum<Meta>, ::iqsm::binding::resource::Ptr) -> Id<Meta>;

    template<meta::Binding Meta>
    auto loaded(Manager, const Id<Meta>&) -> bool;

    template<meta::Binding Meta>
    auto load(repo::Commit, Manager, const Id<Meta>&, const ::iqsm::binding::resource::Loader<Meta>&) -> bool;
}

namespace iqsm::helpers::resource {
    template<meta::Binding Meta>
    auto declare(repo::Commit commit, Quantum<Meta> quantum) -> Id<Meta> {
        return helpers::binding::declare<Meta>(std::move(commit), std::move(quantum));
    }

    template<meta::Binding Meta>
    auto create(
        repo::Commit commit,
        Manager manager,
        Quantum<Meta> quantum,
        ::iqsm::binding::resource::Ptr external) -> Id<Meta>
    {
        return manager->layer<Meta>()->create(std::move(commit), std::move(quantum), std::move(external));
    }

    template<meta::Binding Meta>
    auto loaded(Manager manager, const Id<Meta>& id) -> bool
    {
        return manager->layer<Meta>()->try_get(id) != nullptr;
    }

    template<meta::Binding Meta>
    auto load(
        repo::Commit commit,
        Manager manager,
        const Id<Meta>& id,
        const ::iqsm::binding::resource::Loader<Meta>& loader) -> bool
    {
        if (loaded<Meta>(manager, id)) return false;
        if (!helpers::particle::exists<Meta>(commit.initial, id)) return false;

        const auto& quantum = helpers::particle::get<Meta>(commit.initial, id);
        auto external = loader.load(quantum.passport);
        if (!external) return false;

        manager->layer<Meta>()->bind(id, std::move(external));
        return true;
    }
}

