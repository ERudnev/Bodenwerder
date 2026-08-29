#pragma once

#include <eltanin/locality/geo/rock.q1.h>
#include <rmmr/resources/builders/geometryGenerator.h>

namespace eltanin::locality::geo {

    auto generateRockVolume(const GeneralizedRecipe&) -> Volume;
    auto rockSdf(const GeneralizedRecipe&, vec3) -> float;
    auto generateLavaBrickVolume() -> Volume;
    auto applyLavaBrickHeat(rmmr::resource::builders::geometry::CpuPresentation&) -> void;
    auto generateIceBlobVolume() -> Volume;
    auto applyIceBlobSinter(rmmr::resource::builders::geometry::CpuPresentation&) -> void;

}
