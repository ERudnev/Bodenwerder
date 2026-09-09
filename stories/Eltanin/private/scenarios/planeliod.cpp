#include "scenarios/planeliod.h"

#include "geo/celestial/planetiod.h"
#include "geo/celestial/sun.h"
#include <eltanin/world.q1.h>

namespace eltanin::scenario {

    using namespace rmmr;

    void Planeliod::loadResources(Writing, const rmmr::wrapper::assets::Handles&) {
    }

    void Planeliod::populate(Writing context, rmmr::system::Device::Id device) {
        with<World>::placeCamera(context, Pose::from(origin + Pos{0.0f, 80.0f, 200.0f}, HPB{0.0f, -25.0f, 0.0f}));
        locality::geo::Sun::place(context, locality::geo::Sun::sol());
        locality::geo::Planetoid::place(context, device, Pose::from({0,0,0}, HPB{0.0f, 0.0f, 0.0f}), locality::geo::Planetoid::Look{.seed = 7, .radius = 10000.0f, .maxRelief = 80.0f, .surfaceAcceleration = 4.0f, .tectonic = 0.015f, .atmosphereRadius = 14000.0f, .seaDensity = 1200.0f});
    }

}
