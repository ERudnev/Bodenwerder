#pragma once

#include <base/types/common_types.h>

namespace eltanin::mech {

    using namespace base::common_types;

    enum class role {
        custom, // has no defined role
        propulsion, // assumes free back for slots and any orientation for maneuver sockets
        power,
        gyros,
        weaponry, // assumes top or front open space for clots and ny placement for point defence
        cargo, // hangar requires slot+socket for hatch
        logistic, // reserved space to allow access to other parts of the ship, hatches as socket
        emissive,
        control,
        living,
    };
}

namespace eltanin::mech::settings {

    // Actor albedo tint for shared white inner mesh (texture × albedo).
    inline auto colorCode(role val) -> vec3 {
        using role = eltanin::mech::role;
        switch (val) {
            case role::custom: return rgb(130, 130, 130);
            case role::propulsion: return rgb(255, 170, 0);
            case role::power: return rgb(255, 230, 0);
            case role::gyros: return rgb(4, 4, 255);
            case role::weaponry: return rgb(255, 0, 0);
            case role::cargo: return rgb(92, 72, 47);
            case role::logistic: return rgb(1, 72, 21);
            case role::emissive: return rgb(234, 5, 255);
            case role::control: return rgb(0, 229, 245);
            case role::living: return rgb(9, 255, 0);
        }
        return {1.0f, 1.0f, 1.0f};
    }
}
