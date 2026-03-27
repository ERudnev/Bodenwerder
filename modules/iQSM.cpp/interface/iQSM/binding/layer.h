#pragma once

#include <format>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <utility>

#include <iQSM/binding/driver.h>
#include <iQSM/helpers/binding.h>
#include <iQSM/identifier.h>
#include <iQSM/meta/concepts.h>
#include <iQSM/references.h>
#include <iQSM/repository/commit.h>

namespace iqsm::binding {

    struct LayerAbstract {
        virtual ~LayerAbstract()=default;
    };

    template<meta::Binding Meta, typename DriverType>
    struct LayerData final : LayerAbstract {
        using Id = ::iqsm::Id<Meta>;
        using Item = ::iqsm::Item<Meta>;
        using Passport = typename Meta::Passport;
        using Container = std::unordered_map<Id, std::unique_ptr<DriverType>>;

        Container handlers;

        DriverType* try_get(const Id& id) const noexcept;
        DriverType* get(const Id& id) const;

        template<typename... Args>
        Id create(repo::Commit commit, ::iqsm::Quantum<Meta> quantum, Args&&... args);
    };

    template<meta::Binding Meta>
    using Layer = ref<LayerData<Meta, iqsm::detail::binding::driver_t<Meta>>>;

    struct ManagerData final {
        using TypeId = internals::Types::RuntimeId;
        using Container = std::unordered_map<TypeId, ref<LayerAbstract>>;

        Container layers;

        template<meta::Binding Meta>
        Layer<Meta> register_layer();

        template<meta::Binding Meta>
        Layer<Meta> layer();
    };
}

//impl:
namespace iqsm::binding {

    template<meta::Binding Meta>
    Layer<Meta> ManagerData::register_layer() {
        const auto rttid = types::aspectId<Meta>();
        auto created = base::make_shared<LayerData<Meta, iqsm::detail::binding::driver_t<Meta>>>();
        layers.emplace(rttid, created);
        return created;
    }

    template<meta::Binding Meta>
    Layer<Meta> ManagerData::layer() {
        const auto rttid = types::aspectId<Meta>();

        if (auto it = layers.find(rttid); it != layers.end()) {
            return base::shared_ref_cast<LayerData<Meta, iqsm::detail::binding::driver_t<Meta>>>(it->second);
        }

        return register_layer<Meta>();
    }

    template<meta::Binding Meta, typename DriverType>
    DriverType* LayerData<Meta, DriverType>::try_get(const Id& id) const noexcept {
        const auto it = handlers.find(id);
        if (it == handlers.end()) {
            return nullptr;
        }
        return it->second.get();
    }

    template<meta::Binding Meta, typename DriverType>
    DriverType* LayerData<Meta, DriverType>::get(const Id& id) const {
        if (DriverType* p = try_get(id)) {
            return p;
        }
        throw std::runtime_error(
            std::format("iqsm::binding::LayerData: no driver for id {}", id));
    }

    template<meta::Binding Meta, typename DriverType>
    template<typename... Args>
    typename LayerData<Meta, DriverType>::Id LayerData<Meta, DriverType>::create(
        repo::Commit commit,
        ::iqsm::Quantum<Meta> quantum,
        Args&&... args) {
        const Id id = iqsm::helpers::binding::declare<Meta>(std::move(commit), std::move(quantum));

        handlers.emplace(id, std::make_unique<DriverType>(std::forward<Args>(args)...));
        return id;
    }
}