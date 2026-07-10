#pragma once

#include <Raidenmamare/assets/shader.q1.h>
#include <Raidenmamare/materials/uniformSemantics.h>
#include <Raidenmamare/resources/shader.q1.h>
#include <Raidenmamare/system/core.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::resource {

    using namespace fqsm::api;

    struct Material : Entity<Material> {
        using Locations = material::Semantics::RuntimeMapping;

        struct Quantum {
            Shader::Id shader;
            Locations locations;
            vector<asset::Uniform::Binding> bindings;
        };
        struct Actions : BaseActions {
            static void apply(Reading, Id, system::Device::Id);
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Material_group : Group<Material_group, system::Device, Material> {
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
