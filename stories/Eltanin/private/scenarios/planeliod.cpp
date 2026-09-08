#include "scenarios/planeliod.h"

#include <eltanin/world.q1.h>

namespace eltanin::scenario {

    using namespace rmmr;

    void Planeliod::loadResources(Writing, const rmmr::wrapper::assets::Handles&) {
    }

    void Planeliod::populate(Writing context, rmmr::system::Device::Id) {
        with<World>::placeCamera(context, Pose::from(origin + Pos{0.0f, 2135.0f, 100.0f}, HPB{0.0f, -20.0f, 0.0f}));
    }

}
