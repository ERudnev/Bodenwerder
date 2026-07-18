#pragma once

#include <array>
#include <cstddef>
#include <functional>
#include <type_traits>
#include <vector>

namespace rmmr::renderer {

    enum class Pass {
        opaque,
        transparent,
        shadow,
        ui,
        gizmo,
        identity, // keep last: bounds SeparateBuffers
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
