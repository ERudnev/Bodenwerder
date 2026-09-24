#pragma once

#include <rmmr/renderer/gl.q1.h>
#include <rmmr/resources/manager.q1.h>

#include <cstdint>
#include <cstddef>
#include <span>

#include <fQSM/api/interface.h>

namespace rmmr::resource::texture {

    using namespace fqsm::api;

    using Reference = resource::Unit::Reference;

    struct Runtime : Entity<Runtime> {
        struct Quantum {
            system::Device::Id device;
            renderer::Texture handle;
            index2 size;
        };
        struct Internals;
        static const Behavior customAspectReactions();
    };

    struct Asset : Feature<Asset, resource::Unit> {
        enum class Format : std::uint8_t {
            r16Snorm,
            rg8,
            rgba8,
        };
        enum class Sampling : std::uint8_t {
            nearest,
            linear,
        };
        struct Quantum {};
        struct Actions : BaseActions {
            static auto install(Writing, Id, system::Device::Id, Format, index2 size, std::span<const std::byte>) -> optional<Runtime::Id>;
            static auto install(Writing, Id, system::Device::Id, Format, Sampling, index2 size, integer layers, integer levels, std::span<const std::byte>) -> optional<Runtime::Id>;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Loader : Feature<Loader, Asset> {
        struct Quantum {
            filename file;
            bool mipmaps = true;
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

    // Schema fragment of doctrine/resources/textures.q1: every aspect declared in this file.
    namespace doctrine {
        auto textures() -> Schema;
    }

}
