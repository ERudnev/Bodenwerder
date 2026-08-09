#pragma once

#include <rmmr/resources/manager.q1.h>
#include <rmmr/semantics.q1.h>
#include <rmmr/resources/shaders.q1.h>
#include <rmmr/system/core.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::resource::material {

    using namespace fqsm::api;
    using Reference = resource::Unit::Reference;
    using Uniform = ::rmmr::resource::Uniform;

    // Material = pass techniques (shader + uniform slots + blend). Sampler values are on the draw.
    struct Runtime : Entity<Runtime> {
        using Locations = ::rmmr::material::Semantics::RuntimeMapping;
        struct Technique {
            shader::Runtime::Id shader;
            Locations locations;
            vector<Uniform::Binding> bindings;
        };
        struct Quantum {
            umap<renderer::Pass, Technique> techniques;
            bool nearest;
            renderer::BlendMode blend;
        };
        struct Actions : BaseActions {
            static void apply(Reading, Id, system::Device::Id, renderer::Pass);
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Asset : Feature<Asset, resource::Unit> {
        struct Technique {
            shader::Reference program;
            Uniform::Palette uniforms;
        };
        struct Quantum {
            umap<renderer::Pass, Technique> techniques;
            bool nearest;
            renderer::BlendMode blend;
        };
        struct Actions : BaseActions {
            static auto materialize(Writing, Id, system::Device::Id) -> optional<Runtime::Id>;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    // Material + sampler params; albedoMap value = texpack layer filename.
    struct Instance {
        Asset::Id material;
        umap<string, string> textures;
    };

}
