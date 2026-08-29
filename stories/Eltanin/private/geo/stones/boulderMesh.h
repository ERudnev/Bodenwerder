#pragma once

#include <eltanin/locality/geo/rock.q1.h>
#include <rmmr/resources/builders/geometryGenerator.h>

namespace eltanin::locality::geo {

    auto meshDebris(const GeneralizedRecipe&) -> rmmr::resource::builders::geometry::CpuPresentation;

}
