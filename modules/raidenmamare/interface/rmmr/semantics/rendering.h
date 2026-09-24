#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <vector>

namespace rmmr::renderer {

    enum class Pass {
        opaque,
        transparent,
        shadow,
        gizmo,
        sprite,
        environment, // sky / celestial; after opaque so far mesh Z can fail against locality
        atmosphere, // spherical fog/limb over HDR + depth; before identity
        identitySelected, // selected Identified → selected-ID buffer (before identity; shared depth)
        identity, // keep last: bounds SeparateBuffers
    };

    // Per-draw / per-material; not a queue basket. inherit → pass default.
    enum class BlendMode : std::uint8_t {
        inherit,
        replace,
        alpha,
        additive,
        premultiplied, // ONE, ONE_MINUS_SRC_ALPHA (add when a≈0; occlude when a>0)
    };

    enum class ToggleMode : std::uint8_t {
        inherit,
        disabled,
        enabled,
    };

    enum class DepthCompare : std::uint8_t {
        inherit,
        less,
        lessEqual,
        equal,
        notEqual,
        greaterEqual,
        greater,
        always,
        never,
    };

    enum class LightingMode : std::uint8_t {
        unlit,
        primary,
    };

    struct RenderState {
        BlendMode blend;
        ToggleMode depthTest;
        ToggleMode depthWrite;
        DepthCompare depthCompare;
    };

    inline constexpr std::size_t pass_count = static_cast<std::size_t>(Pass::identity) + 1;

    using Passes = std::vector<Pass>;

    // Dense pass-keyed baskets without unordered_map; T stays at call site (e.g. Command).
    template<typename T>
    struct SeparateBuffers {
        using Buffer = std::vector<T>;

        std::array<Buffer, pass_count> buffers{};

        auto operator[](Pass pass) -> Buffer& {
            return buffers[static_cast<std::size_t>(pass)];
        }

        auto operator[](Pass pass) const -> const Buffer& {
            return buffers[static_cast<std::size_t>(pass)];
        }
    };

    namespace PassesPresets {

        inline const Passes opaque_only{Pass::opaque};
        inline const Passes opaque_casting{Pass::opaque, Pass::shadow};
        inline const Passes transparent_only{Pass::transparent};
        inline const Passes gizmo_only{Pass::gizmo};
        inline const Passes shadow_only{Pass::shadow};
        inline const Passes sprite_only{Pass::sprite};
        inline const Passes environment_only{Pass::environment};
        inline const Passes identity_only{Pass::identity};
        inline const Passes identity_selected_only{Pass::identitySelected};
        inline const Passes atmosphere_only{Pass::atmosphere};

    }

}

namespace std {

    template<>
    struct hash<rmmr::renderer::Pass> {
        auto operator()(rmmr::renderer::Pass pass) const noexcept -> size_t {
            using underlying = underlying_type_t<rmmr::renderer::Pass>;
            return hash<underlying>{}(static_cast<underlying>(pass));
        }
    };

}
