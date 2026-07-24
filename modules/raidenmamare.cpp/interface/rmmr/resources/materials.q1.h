#pragma once

#include <utility>

#include <rmmr/resources/manager.q1.h>
#include <rmmr/semantics.q1.h>
#include <rmmr/resources/shaders.q1.h>
#include <rmmr/resources/textures.q1.h>
#include <rmmr/system/core.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::resource::material {

    using namespace fqsm::api;

    using Reference = resource::Unit::Reference;

    struct Asset : Feature<Asset, resource::Unit> {
        struct TextureBinding {
            Uniform::Id uniform;
            texture::Reference texture;
        };
        struct Technique {
            shader::Reference program;
            Uniform::Palette uniforms;
            vector<TextureBinding> textures;
        };
        struct Quantum {
            umap<renderer::Pass, Technique> techniques;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Runtime : Entity<Runtime> {
        using Locations = ::rmmr::material::Semantics::RuntimeMapping;
        struct TextureBinding {
            Uniform::Id uniform;
            texture::Runtime::Id texture;
        };
        struct Technique {
            shader::Runtime::Id shader;
            Locations locations;
            vector<Uniform::Binding> bindings;
            vector<TextureBinding> textures;
        };
        struct Quantum {
            umap<renderer::Pass, Technique> techniques;
        };
        struct Actions : BaseActions {
            static void apply(Reading, Id, system::Device::Id, renderer::Pass);
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Composer : Feature<Composer, Asset> {
        struct Quantum {};
        struct Actions : BaseActions {
            static auto materialize(Writing, Id, system::Device::Id) -> optional<Runtime::Id>;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}

namespace fqsm::aspect {

template<>
struct Retrospection<rmmr::resource::material::Asset::TextureBinding> {
    template<typename Desc>
    static void describe(Desc& d) {
        d.aspect("TextureBinding");
        d.one(field<&rmmr::resource::material::Asset::TextureBinding::uniform>("uniform"));
        d.one(field<&rmmr::resource::material::Asset::TextureBinding::texture>("texture"));
    }
};

template<>
struct Retrospection<rmmr::resource::material::Asset::Technique> {
    template<typename Desc>
    static void describe(Desc& d) {
        d.aspect("Technique");
        d.one(field<&rmmr::resource::material::Asset::Technique::program>("program"));
        d.one(collection<rmmr::resource::Uniform::Id, &rmmr::resource::material::Asset::Technique::uniforms>("uniforms"));
        d.one(collection<rmmr::resource::material::Asset::TextureBinding, &rmmr::resource::material::Asset::Technique::textures>("textures"));
    }
};

template<>
struct Retrospection<rmmr::resource::material::Asset> {
    template<typename Desc>
    static void describe(Desc& d) {
        d.aspect("rmmr::resource::material::Asset");
        d.one(collection<std::pair<rmmr::renderer::Pass, rmmr::resource::material::Asset::Technique>, &rmmr::resource::material::Asset::Quantum::techniques>("techniques", "pass"));
    }
};

template<>
struct Retrospection<rmmr::resource::material::Composer> {
    template<typename Desc>
    static void describe(Desc& d) {
        d.aspect("rmmr::resource::material::Composer");
    }
};

}
