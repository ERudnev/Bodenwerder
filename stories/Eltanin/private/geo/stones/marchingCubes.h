#pragma once

#include <eltanin/geo/rock.q1.h>
#include <rmmr/resources/builders/geometryGenerator.h>

namespace eltanin::geo {

    auto meshVolume(const Volume&) -> rmmr::resource::builders::geometry::CpuPresentation;

}
