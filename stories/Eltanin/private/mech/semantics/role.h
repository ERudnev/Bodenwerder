#pragma once

#include <eltanin/mech/semantics.q1.h>

#include <base/types/common_types.h>

namespace eltanin::mech::settings {

    using namespace base::common_types;

    // Actor albedo tint for shared white inner mesh (texture × albedo).
    inline auto colorCode(Role val) -> vec3 {
        switch (val) {
            case Role::custom: return rgb(130, 130, 130);
            case Role::propulsion: return rgb(255, 170, 0);
            case Role::power: return rgb(255, 230, 0);
            case Role::gyros: return rgb(4, 4, 255);
            case Role::weaponry: return rgb(255, 0, 0);
            case Role::cargo: return rgb(92, 72, 47);
            case Role::logistic: return rgb(1, 72, 21);
            case Role::emissive: return rgb(234, 5, 255);
            case Role::control: return rgb(0, 229, 245);
            case Role::living: return rgb(9, 255, 0);
        }
        return {1.0f, 1.0f, 1.0f};
    }

}
