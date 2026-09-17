#include "scenarios/planeliod.h"

#include "geo/celestial/sun.h"
#include <eltanin/geo/minerals.q1.h>
#include <eltanin/world.q1.h>
#include <rmmr/controller/camera3d.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/gizmos.q1.h>

namespace eltanin::scenario {

    using namespace rmmr;

    void Planeliod::loadResources(Writing, const rmmr::wrapper::assets::Handles&) {
    }

    void Planeliod::populate(Writing context, rmmr::system::Device::Id) {
        with<World>::placeCamera(context, Pose::from(Pos{0.0f, 0.0f, 65000.0f}, HPB{0.0f, 0.0f, 0.0f}));
        if (const auto camera = with<World>::get_global(context).camera)
            with<controller::Camera3d>::modify(context, *camera)->moveScale = 100.0f;
        for (auto [gridId, _] : context->aspect<scene::Grid>().items()) {
            auto mesh = with<scene::actor::MeshState>::modify(context, gridId);
            mesh->scale = vec3{100.0f};
            mesh->patternScale = 1.0f;
            with<scene::Grid>::modify(context, gridId)->patternScale = 1.0f;
        }
        geo::Sun::place(context, geo::Sun::sol());
    }

    void Planeliod::placePlanet(Writing context, rmmr::system::Device::Id device, base::maybe<planet::Planet>& planet) {
        constexpr float radius = 34000.0f; // close to real Mars, divided by 100
        using Mineral = geo::Mineral::Kind;
        auto nibble = [](Mineral channel, integer fill) -> geo::Mineral::Mix {
            return geo::Mineral::Mix{static_cast<std::uint64_t>(fill)} << (static_cast<integer>(channel) * 4);
        };
        planet.emplace(
            planet::Passport{
                .seed = 7,
                .radius = radius,
                .surfaceAcceleration = 3.71f,
                .orientation = quat{1.0f, 0.0f, 0.0f, 0.0f},
                .spinPeriod = 88200.0f, // Mars sidereal day, seconds
                .geology = {
                    .mix = nibble(Mineral::Pyroxene, 12) | nibble(Mineral::Olivine, 8) | nibble(Mineral::Feldspar, 6) | nibble(Mineral::Oxides, 11) | nibble(Mineral::Clay, 7) | nibble(Mineral::Ice, 6) | nibble(Mineral::Salts, 4) | nibble(Mineral::Iron, 5) | nibble(Mineral::Carbonaceous, 3),
                    .differentiation = 0.62f,
                    .surfaceAge = 0.78f,
                    .cohesion = 0.48f,
                    .grain = 0.55f,
                    .tectonic = 0.22f,
                    .amplitude = 2000.0f,
                },
                .atmosphere = {
                    .outerRadius = radius + 1500.0f,
                    .seaDensity = 300.0f,
                    .kerman = 500.0f,
                    .day = RGB{0.78f, 0.52f, 0.36f},
                },
            },
            planet::Planet::recommendedDetail(radius, 32.0f));
        planet->place(context, device, Pose::from(Pos{0.0f, 0.0f, 0.0f}, HPB{0.0f, 0.0f, 0.0f}));
    }

}
