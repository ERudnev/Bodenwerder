#pragma once

#include "physics/collisions.h"

namespace eltanin::phys::collision {

    auto pairKey(Body::Id first, Body::Id second) -> PairKey;
    void prepareResting(State&, Stewarding);
    void acquireResting(State&, Stewarding);
    void recheckResting(State&, Stewarding);
    void solveRestingIslands(State&, Stewarding);

}
