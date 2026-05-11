#pragma once

#include <stdexcept>
#include <utility>
#include <memory>

#include <iQSM/references.h>
#include <iQSM/state/layer.h>
#include <iQSM/state/view.h>
#include <iQSM/state/schema.h>

namespace iqsm::state {
    
    // DeltaData is "patch" of "single-version" (mutable) containers if (mutable+immutable Slices)
    template<policy::versioning SliceVersioning>
    struct WorldTemplate : View {
        using VersionedLayer = Layer<policy::order::state, policy::versioning::shared, SliceVersioning>;
        using OperationalLayer = Layer<policy::order::state, policy::versioning::single, SliceVersioning>;
        using OperationalLayerPtr = SlicesLayout<SliceVersioning>::template RefQualified<OperationalLayer>;

        VersionedLayer versioned;
        OperationalLayerPtr operational;

        WorldTemplate(Schema schema);
        auto slice(RAId runtimeTypeId) const -> cref<slice::AbstractState> override;
    };

    // placeholder
    struct WorldData : WorldTemplate<policy::versioning::shared>, std::enable_shared_from_this<WorldData>
    {
        // c-tor
        using WorldTemplate<policy::versioning::shared>::WorldTemplate;

        World share() const override { return World(shared_from_this()); }
        auto clone() const -> ref<WorldData> {
            auto cloned = base::make_shared<WorldData>(schema);
            cloned->versioned = versioned;
            cloned->operational = operational;
            return ref<WorldData>(std::move(cloned));
        }
    };
}


// impl:
namespace iqsm::state {

    template<policy::versioning SliceVersioning>
    WorldTemplate<SliceVersioning>::WorldTemplate(Schema schema)
        : View(std::move(schema)), operational([]() -> OperationalLayerPtr {
            auto runtime = base::make_shared<OperationalLayer>();

            if constexpr (SliceVersioning == policy::versioning::shared) {
                return iqsm::freeze(runtime);
            } else {
                return runtime;
            }
        }())
    {}

    template<policy::versioning SliceVersioning>
    auto WorldTemplate<SliceVersioning>::slice(RAId runtimeTypeId) const -> cref<slice::Abstract> {
        const auto& aspect = schema->aspects.at(runtimeTypeId);

        switch (aspect.layer) {
        case policy::versioning::shared: {
            const auto it = versioned.slices.find(runtimeTypeId);
            return it != versioned.slices.end() ? it->second : aspect.zero;
        }
        case policy::versioning::single: {
            const auto it = operational->slices.find(runtimeTypeId);
            return it != operational->slices.end() ? it->second : aspect.zero;
        }
        }

        throw std::runtime_error("state::WorldTemplate::slice(): unsupported layer policy");
    }
}