#include "scenarios/planeliod.h"

#include "geo/celestial/planetiod.h"
#include "geo/celestial/sun.h"
#include <eltanin/world.q1.h>

#include <cmath>

namespace eltanin::scenario {

    using namespace rmmr;

    void Planeliod::loadResources(Writing, const rmmr::wrapper::assets::Handles&) {
    }

    void Planeliod::populate(Writing context, rmmr::system::Device::Id device) {
        const auto look = locality::geo::Planetoid::Look{.seed = 7, .radius = 10000.0f, .maxRelief = 160.0f, .surfaceAcceleration = 4.0f, .tectonic = 0.028f, .atmosphere = {.radius = 11400.0f, .seaDensity = 1000.0f, .kerman = 1000.0f, .day = RGB{0.42f, 0.62f, 1.00f}}, .terrainTest = locality::geo::Landscape::TerrainTest::off, .testRims = true};
        const vec3 basin = locality::geo::Planetoid::largestBasinDirection(look);
        const vec3 up = std::abs(basin.y) < 0.92f ? vec3{0.0f, 1.0f, 0.0f} : vec3{1.0f, 0.0f, 0.0f};
        const vec3 across = glm::normalize(glm::cross(up, basin));
        const vec3 cameraDirection = glm::normalize(basin + across * 0.13f + up * 0.06f);
        const vec3 forward = -cameraDirection;
        const HPB cameraHpb{
            glm::degrees(std::atan2(forward.x, -forward.z)),
            glm::degrees(std::asin(glm::clamp(forward.y, -1.0f, 1.0f))),
            0.0f,
        };
        with<World>::placeCamera(context, Pose::from(Pos{cameraDirection * (look.radius * 1.98f)}, cameraHpb));
        auto sun = locality::geo::Sun::sol();
        sun.heading = HPB{cameraHpb.x - 68.0f, -30.0f, 0.0f};
        locality::geo::Sun::place(context, sun);
        locality::geo::Planetoid::place(context, device, Pose::from({0,0,0}, HPB{0.0f, 0.0f, 0.0f}), look);
    }

}
