#pragma once

#include <eltanin/geo/rock.q1.h>
#include <rmmr/resources/builders/geometryGenerator.h>

namespace eltanin::geo {

    auto generateRockVolume(const Recipe&) -> Volume;
    auto rockSdf(const Recipe&, vec3) -> float;
    auto generateLavaBrickVolume() -> Volume;
    auto applyLavaBrickHeat(rmmr::resource::builders::geometry::CpuPresentation&) -> void;

}
