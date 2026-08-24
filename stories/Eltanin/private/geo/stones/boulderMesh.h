#pragma once

#include <eltanin/geo/rock.q1.h>
#include <rmmr/resources/builders/geometryGenerator.h>

namespace eltanin::geo {

    auto meshDebris(const GeneralizedRecipe&) -> rmmr::resource::builders::geometry::CpuPresentation;

}
