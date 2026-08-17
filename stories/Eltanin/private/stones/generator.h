#pragma once

#include <eltanin/geo/rock.q1.h>

namespace eltanin::geo {

    auto generateRockVolume(const Recipe&) -> Volume;
    auto rockSdf(const Recipe&, vec3) -> float;

}
