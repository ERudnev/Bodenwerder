#pragma once

#include <eltanin/geo/boulder.q1.h>
#include <rmmr/resources/builders/geometryGenerator.h>

namespace eltanin::geo {

    auto meshBoulder(const Boulder::Recipe&) -> rmmr::resource::builders::geometry::CpuPresentation;

}
