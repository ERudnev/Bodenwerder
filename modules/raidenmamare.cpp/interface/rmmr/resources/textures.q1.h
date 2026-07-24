#pragma once

#include <rmmr/renderer/gl.q1.h>
#include <rmmr/resources/manager.q1.h>

#include <cstdint>

#include <fQSM/api/interface.h>

namespace rmmr::resource::texture {

    using namespace fqsm::api;

    using Reference = resource::Unit::Reference;

    struct Asset : Feature<Asset, resource::Unit> {
        struct Quantum {};
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Runtime : Entity<Runtime> {
        struct Quantum {
            system::Device::Id device;
            renderer::Texture handle;
            index2 size;
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

    struct Loader : Feature<Loader, Asset> {
        struct Quantum {
            filename file;
        };
        struct Actions : BaseActions {
            static auto materialize(Writing, Id, system::Device::Id) -> optional<Runtime::Id>;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Generator : Feature<Generator, Asset> {
        enum class Pattern : std::uint8_t {
            whiteCircle,
            whiteRing,
        };
        struct Quantum {
            index2 size;
            Pattern pattern = Pattern::whiteCircle;
        };
        struct Actions : BaseActions {
            static auto materialize(Writing, Id, system::Device::Id) -> optional<Runtime::Id>;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}

namespace fqsm::aspect {

template<>
struct Retrospection<rmmr::resource::texture::Asset> {
    template<typename Desc>
    static void describe(Desc& d) {
        d.aspect("rmmr::resource::texture::Asset");
    }
};

template<>
struct Retrospection<rmmr::resource::texture::Loader> {
    template<typename Desc>
    static void describe(Desc& d) {
        d.aspect("rmmr::resource::texture::Loader");
        d.one(field<&rmmr::resource::texture::Loader::Quantum::file>("file"));
    }
};

template<>
struct Retrospection<rmmr::resource::texture::Generator> {
    template<typename Desc>
    static void describe(Desc& d) {
        d.aspect("rmmr::resource::texture::Generator");
        d.one(field<&rmmr::resource::texture::Generator::Quantum::size>("size"));
        d.one(field<&rmmr::resource::texture::Generator::Quantum::pattern>("pattern"));
    }
};

}
