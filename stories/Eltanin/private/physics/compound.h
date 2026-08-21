#pragma once

#include <eltanin/geo/rock.q1.h>
#include <eltanin/physics/rigid.q1.h>
#include <rmmr/resources/builders/geometryGenerator.h>

namespace eltanin::phys::rigid {

    auto octaCompound() -> Compound;
    auto wrapCompound(const vector<vec3>& shape, vec3 restCom) -> Compound;
    auto volumeCompound(const geo::Volume&, const vector<vec3>& shape, vec3 restCom) -> Compound;
    auto debugMeshFromCompound(const Compound&, const vector<vec3>& shape) -> rmmr::resource::builders::geometry::CpuPresentation;

}
