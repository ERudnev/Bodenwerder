#pragma once

#include <format>
#include <stdexcept>
#include <unordered_map>
#include <utility>

#include <iQSM/types.h>

namespace iqsm::resources {
    template<typename Meta>
    struct Layer {
        using Id = typename Meta::Id;
        using RuntimeStorage = typename Meta::RuntimeStorage;
        using RuntimeAccess = typename Meta::RuntimeAccess;
        using Container = std::unordered_map<Id, RuntimeStorage>;

        Container values;

        auto materialized(const Id& id) const noexcept -> bool {
            return values.contains(id);
        }

        auto value(const Id& id) -> RuntimeStorage& {
            const auto it = values.find(id);
            if (it != values.end()) return it->second;
            throw std::runtime_error(
                std::format("iqsm::resources::Layer: no runtime value for type {} and id {}", internals::type_name<Meta>(), id));
        }

        auto value(const Id& id) const -> const RuntimeStorage& {
            const auto it = values.find(id);
            if (it != values.end()) return it->second;
            throw std::runtime_error(
                std::format("iqsm::resources::Layer: no runtime value for type {} and id {}", internals::type_name<Meta>(), id));
        }

        auto provide(const Id& id) -> RuntimeAccess {
            return value(id);
        }

        auto provide(const Id& id) const -> RuntimeAccess {
            return value(id);
        }

        void materialize(const Id& id, RuntimeStorage value) {
            if (materialized(id)) {
                throw std::runtime_error(
                    std::format("iqsm::resources::Layer: runtime value already exists for type {} and id {}", internals::type_name<Meta>(), id));
            }
            values.emplace(id, std::move(value));
        }

        void release(const Id& id) {
            if (values.erase(id) > 0) return;
            throw std::runtime_error(
                std::format("iqsm::resources::Layer: no runtime value to release for type {} and id {}", internals::type_name<Meta>(), id));
        }
    };
}
