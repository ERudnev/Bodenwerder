#pragma once

#include <unordered_map>

#include <fQSM/meta/interface.include.h>
#include <fQSM/meta/rtid.h>
#include <fQSM/references.h>
#include <fQSM/schema/binding.h>
#include <fQSM/schema/dag.h>
#include <fQSM/state/details/aware_at.h>
#include <fQSM/state/slice/erased.h>

namespace fqsm::state {

    template<auto Spawn>
    concept BindingSpawn = requires(schema::Binding& binding) {
        { (binding.*Spawn)() } -> std::convertible_to<ref<slice::Erased>>;
    };

    template<BindingSpawn Spawn>
    struct Composite {
        const Schema schema;

        explicit Composite(Schema schema) : schema{std::move(schema)} {}

        auto slice(aspect::Rtid runtimeTypeId) const -> cref<slice::Erased> {
            return freeze(ensure(runtimeTypeId));
        }

        auto slice(aspect::Rtid runtimeTypeId) -> ref<slice::Erased> {
            return ensure(runtimeTypeId);
        }

    private:
        auto ensure(aspect::Rtid runtimeTypeId) const -> ref<slice::Erased> {
            if (const auto found = slices.find(runtimeTypeId); found != slices.end()) {
                return found->second;
            }

            auto& binding = aware_at(schema->nodes, runtimeTypeId).binding;
            return slices.emplace(runtimeTypeId, (binding.*Spawn)()).first->second;
        }

        using Slices = std::unordered_map<aspect::Rtid, ref<slice::Erased>, aspect::Rtid::Hash>;

        mutable Slices slices;
    };

}
