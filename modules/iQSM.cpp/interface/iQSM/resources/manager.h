#pragma once

#include <format>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <utility>

#include <iQSM/schema.h>
#include <iQSM/meta/aspect_id.h>
#include <iQSM/resources/layer.h>
#include <iQSM/resources/materializer.h>
#include <iQSM/resources/slot.h>
#include <iQSM/types.h>

namespace iqsm::resources {

    struct ManagerCore {
        using TypeId = types::RuntimeId;
        Schema schema;
        std::unordered_map<TypeId, std::unique_ptr<resources::SlotAbstract>> slots;

        explicit ManagerCore(Schema schema)
            : schema(std::move(schema)) {}

        template<typename Meta>
        auto slot() -> resources::Slot<Meta>&;

        template<typename Meta>
        auto slot() const -> const resources::Slot<Meta>&;

        template<typename Meta>
        auto layer() -> Layer<Meta>&;

        template<typename Meta>
        auto layer() const -> const Layer<Meta>&;

        template<typename Meta>
        auto materialized(const typename Meta::Id& id) const noexcept -> bool;

        template<typename Meta>
        auto materializer() -> resources::Materializer<Meta>&;
    };

}

namespace iqsm::resources {
    template<typename Meta>
    auto ManagerCore::slot() -> resources::Slot<Meta>& {
        const auto type_id = types::aspectId<Meta>();
        const auto it = slots.find(type_id);
        if (it != slots.end()) {
            auto* typed = dynamic_cast<resources::Slot<Meta>*>(it->second.get());
            if (!typed) {
                throw std::runtime_error(
                    std::format("iqsm::resources::ManagerCore: slot type mismatch for {}", internals::type_name<Meta>()));
            }
            return *typed;
        }

        const auto schema_it = schema->aspects.find(type_id);
        if (schema_it == schema->aspects.end()) {
            throw std::runtime_error(
                std::format("iqsm::resources::ManagerCore: type {} is not registered in schema", internals::type_name<Meta>()));
        }

        const auto& resource = schema_it->second.resource;
        if (!resource.create_slot) {
            throw std::runtime_error(
                std::format("iqsm::resources::ManagerCore: no runtime slot for type {}", internals::type_name<Meta>()));
        }

        auto created = resource.create_slot();
        auto* typed = dynamic_cast<resources::Slot<Meta>*>(created.get());
        if (!typed) {
            throw std::runtime_error(
                std::format("iqsm::resources::ManagerCore: schema slot factory mismatch for {}", internals::type_name<Meta>()));
        }

        auto* raw = typed;
        slots.emplace(type_id, std::move(created));
        return *raw;
    }

    template<typename Meta>
    auto ManagerCore::slot() const -> const resources::Slot<Meta>& {
        const auto it = slots.find(types::aspectId<Meta>());
        if (it == slots.end()) {
            throw std::runtime_error(
                std::format("iqsm::resources::ManagerCore: no runtime slot for type {}", internals::type_name<Meta>()));
        }

        const auto* typed = dynamic_cast<const resources::Slot<Meta>*>(it->second.get());
        if (!typed) {
            throw std::runtime_error(
                std::format("iqsm::resources::ManagerCore: slot type mismatch for {}", internals::type_name<Meta>()));
        }

        return *typed;
    }

    template<typename Meta>
    auto ManagerCore::layer() -> Layer<Meta>& {
        return slot<Meta>().layer;
    }

    template<typename Meta>
    auto ManagerCore::layer() const -> const Layer<Meta>& {
        return slot<Meta>().layer;
    }

    template<typename Meta>
    auto ManagerCore::materialized(const typename Meta::Id& id) const noexcept -> bool {
        const auto it = slots.find(types::aspectId<Meta>());
        if (it != slots.end()) {
            const auto* typed = dynamic_cast<const resources::Slot<Meta>*>(it->second.get());
            if (!typed) return false;
            return typed->layer.materialized(id);
        }
        return false;
    }

    template<typename Meta>
    auto ManagerCore::materializer() -> resources::Materializer<Meta>& {
        return *slot<Meta>().materializer;
    }
}