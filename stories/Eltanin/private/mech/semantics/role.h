#pragma once

#include <eltanin/mech/semantics.q1.h>

#include <base/types/common_types.h>

namespace eltanin::mech::settings {

    using namespace base::common_types;

    // Actor albedo tint for shared white inner mesh (texture × albedo).
    inline auto colorCode(role val) -> vec3 {
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
