#pragma once

#include <stdexcept>
#include <utility>

#include <iQSM/state/layer.h>
#include <iQSM/state/view.h>

namespace iqsm::state {
    
    // DeltaData is "patch" of "single-version" (mutable) containers if (mutable+immutable Slices)
    template<policy::versioning SliceVersioning>
    struct WorldTemplate : View {
        using VersionedLayer = Layer<policy::role::value, policy::versioning::shared, SliceVersioning>;
        using OperationalLayer = Layer<policy::role::value, policy::versioning::single, SliceVersioning>;
        using OperationalLayerPtr = SlicesLayout<SliceVersioning>::template RefQualified<OperationalLayer>;

        VersionedLayer versioned;
        OperationalLayerPtr operational;

        WorldTemplate(Schema schema);
        auto slice(RAId runtimeTypeId) const -> cref<slice::Abstract> override;
    };

    // placeholder
    struct WorldData : WorldTemplate<policy::versioning::shared> 
    {
        using WorldTemplate<policy::versioning::shared>::WorldTemplate;
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

        try {
            switch (aspect.layer) {
            case policy::versioning::shared:
                return versioned.slices.at(runtimeTypeId);
            case policy::versioning::single:
                return operational->slices.at(runtimeTypeId);
            }
        } catch (const std::out_of_range&) {
            throw std::out_of_range("state::WorldTemplate::slice(): required slice not found in schema-selected layer");
        }

        throw std::runtime_error("state::WorldTemplate::slice(): unsupported layer policy");
    }
}