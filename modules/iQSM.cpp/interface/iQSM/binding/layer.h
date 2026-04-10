#pragma once

#include <format>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <utility>

#include <iQSM/binding/resource.h>
#include <iQSM/helpers/binding.h>
#include <iQSM/identifier.h>
#include <iQSM/meta/concepts.h>
#include <iQSM/references.h>
#include <iQSM/repository/commit.h>

namespace iqsm::binding {

    struct LayerAbstract {
        virtual ~LayerAbstract()=default;
    };

    template<meta::Binding Meta>
    struct LayerData final : LayerAbstract {
        using Id = ::iqsm::Id<Meta>;
        using Item = ::iqsm::Item<Meta>;
        using Passport = typename Meta::Passport;
        using Data = ::iqsm::binding::resource::Data;
        using Container = std::unordered_map<Id, std::unique_ptr<Data>>;

        Container handlers;

        Data* try_get(const Id& id) const noexcept;
        Data* get(const Id& id) const;
        void bind(const Id& id, std::unique_ptr<Data> data);
        bool unbind(const Id& id);

        Id create(repo::Commit commit, ::iqsm::Quantum<Meta> quantum, std::unique_ptr<Data> data);
    };

    template<meta::Binding Meta>
    using Layer = cref<LayerData<Meta>>;

    struct ManagerData final {
        using TypeId = internals::Types::RuntimeId;
        using Container = std::unordered_map<TypeId, ref<LayerAbstract>>;

        Container layers;

        template<meta::Binding Meta>
        ref<LayerData<Meta>> register_layer();

        template<meta::Binding Meta>
        ref<LayerData<Meta>> layer();

        template<meta::Binding Meta>
        Layer<Meta> layer() const;
    };
}

//impl:
namespace iqsm::binding {

    template<meta::Binding Meta>
    ref<LayerData<Meta>> ManagerData::register_layer() {
        const auto rttid = types::aspectId<Meta>();
        auto created = base::make_shared<LayerData<Meta>>();
        layers.emplace(rttid, created);
        return created;
    }

    template<meta::Binding Meta>
    ref<LayerData<Meta>> ManagerData::layer() {
        const auto rttid = types::aspectId<Meta>();

        if (auto it = layers.find(rttid); it != layers.end()) {
            return base::shared_ref_cast<LayerData<Meta>>(it->second);
        }

        return register_layer<Meta>();
    }

    template<meta::Binding Meta>
    Layer<Meta> ManagerData::layer() const {
        const auto rttid = types::aspectId<Meta>();

        if (auto it = layers.find(rttid); it != layers.end()) {
            return base::shared_ref_cast<const LayerData<Meta>>(it->second);
        }

        throw std::runtime_error(std::format("iqsm::binding::ManagerData: no layer for type {}", internals::type_name<Meta>()));
    }

    template<meta::Binding Meta>
    typename LayerData<Meta>::Data* LayerData<Meta>::try_get(const Id& id) const noexcept {
        const auto it = handlers.find(id);
        if (it == handlers.end()) {
            return nullptr;
        }
        return it->second.get();
    }

    template<meta::Binding Meta>
    typename LayerData<Meta>::Data* LayerData<Meta>::get(const Id& id) const {
        if (Data* p = try_get(id)) {
            return p;
        }
        throw std::runtime_error(
            std::format("iqsm::binding::LayerData: no data for id {}", id));
    }

    template<meta::Binding Meta>
    void LayerData<Meta>::bind(const Id& id, std::unique_ptr<Data> data) {
        handlers.insert_or_assign(id, std::move(data));
    }

    template<meta::Binding Meta>
    bool LayerData<Meta>::unbind(const Id& id) {
        return handlers.erase(id) > 0;
    }

    template<meta::Binding Meta>
    typename LayerData<Meta>::Id LayerData<Meta>::create(
        repo::Commit commit,
        ::iqsm::Quantum<Meta> quantum,
        std::unique_ptr<Data> data) {
        const Id id = iqsm::helpers::binding::declare<Meta>(std::move(commit), std::move(quantum));

        handlers.emplace(id, std::move(data));
        return id;
    }
}