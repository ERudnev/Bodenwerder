#pragma once

#include <cstdint>

#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/shaders.q1.h>
#include <rmmr/semantics.q1.h>
#include <rmmr/system/core.q1.h>

#include <fQSM/api/interface.h>

namespace rmmr::resource::overlay {

    using namespace fqsm::api;

    using Reference = resource::Unit::Reference;
    using Uniform = ::rmmr::resource::Uniform;

    // Relative to viewport: effect RT size = viewport / divisor.
    enum class Scale : std::uint8_t {
        full,
        half,
        quarter,
    };

    inline constexpr int selection_capacity = 64;

    inline auto scale_divisor(Scale scale) -> int {
        switch (scale) {
            case Scale::full: return 1;
            case Scale::half: return 2;
            case Scale::quarter: return 4;
        }
        return 1;
    }

    // Screen-space effect. Engine supplies sceneColor / identiffyMap at draw time.
    struct Runtime : Entity<Runtime> {
        struct Quantum {
            shader::Runtime::Id shader;
            vector<Uniform::Binding> bindings;
            Scale scale;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

    struct Asset : Feature<Asset, resource::Unit> {
        struct Quantum {
            shader::Reference program;
            Uniform::Palette uniforms;
            Scale scale;
        };
        struct Actions : BaseActions {
            static auto materialize(Writing, Id, system::Device::Id) -> optional<Runtime::Id>;
        };
        struct Internals : DefaultInternals{};
        static const Behavior customAspectReactions() { return {}; }
    };

}
