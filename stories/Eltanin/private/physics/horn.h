#pragma once

#include <base/types/common_types.h>

#include <vector>

namespace eltanin::phys::horn {

    using namespace base::common_types;

    // Horn absolute orientation (unit-quaternion / symmetric N 4×4 + Jacobi).
    // Inputs already centered. Finds R with world ≈ R·rest (Horn: right ≈ R·left).
    auto orientation(const vector<vec3>& rest_centered, const vector<vec3>& world_centered, const vector<float>& masses) -> quat;

}
