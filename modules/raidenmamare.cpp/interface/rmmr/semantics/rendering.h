#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace rmmr::renderer {

    enum class Pass : std::uint8_t {
        opaque,
        transparent,
        shadow,
        ui,
        gizmo,
        identity,
    };

    using Passes = std::vector<Pass>;

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
            return hash<uint8_t>{}(static_cast<uint8_t>(pass));
        }
    };

}
