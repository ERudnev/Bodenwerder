#pragma once

#include <vector>

namespace rmmr::renderer {

    enum class Pass {
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
