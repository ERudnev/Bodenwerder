#pragma once

#include <eltanin/geo/rock.q1.h>
#include <rmmr/resources/builders/geometryGenerator.h>

namespace eltanin::geo {

    auto generateRockVolume(const Rock::GeneralizedRecipe&) -> Volume;
    auto rockSdf(const Rock::GeneralizedRecipe&, vec3) -> float;
    auto generateLavaBrickVolume() -> Volume;
    auto applyLavaBrickHeat(rmmr::resource::builders::geometry::CpuPresentation&) -> void;
    auto generateIceBlobVolume() -> Volume;
    auto applyIceBlobSinter(rmmr::resource::builders::geometry::CpuPresentation&) -> void;

}
