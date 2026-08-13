#pragma once

#include <vector>

#include <eltanin/mech/semantics.q1.h>

#include "shapes.h"
#include "space.h"
#include "subframe.h"

namespace eltanin::mech::skeleton {

    // World discrete pose: cell lattice pos + compose(cell.ori, local).
    auto worldPose(const space::cell::Pose& cell, space::orient::key local) -> space::cell::Pose;

    // Expand frame::shape → local corners / halfribs (recipes, cell ori 0).
    auto seedCorners(frame::shape) -> std::vector<Corner>;
    auto seedHalfribs(frame::shape) -> std::vector<Halfrib>;

}
