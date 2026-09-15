#include "scenarios/planeliod.h"

#include "geo/celestial/sun.h"
#include <eltanin/locality/geo/minerals.q1.h>
#include <eltanin/world.q1.h>
#include <rmmr/controller/camera3d.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/gizmos.q1.h>

namespace eltanin::scenario {

    using namespace rmmr;

    void Planeliod::loadResources(Writing, const rmmr::wrapper::assets::Handles&) {
    }

    void Planeliod::populate(Writing context, rmmr::system::Device::Id) {
        with<World>::placeCamera(context, Pose::from(Pos{0.0f, 0.0f, 26000.0f}, HPB{0.0f, 0.0f, 0.0f}));
        if (const auto camera = with<World>::get_global(context).camera)
            with<controller::Camera3d>::modify(context, *camera)->moveScale = 25.0f;
        for (auto [gridId, _] : context->aspect<scene::Grid>().items()) {
            auto mesh = with<scene::actor::MeshState>::modify(context, gridId);
            mesh->scale = vec3{1.0f};
            mesh->patternScale = 1.0f;
            with<scene::Grid>::modify(context, gridId)->patternScale = 1.0f;
        }
        locality::geo::Sun::place(context, locality::geo::Sun::sol());
    }

    void Planeliod::placePlanet(Writing context, rmmr::system::Device::Id device, base::maybe<locality::planet::Planet>& planet) {
        constexpr float radius = 1000.0f;
        using Mineral = locality::geo::Mineral::Kind;
        auto nibble = [](Mineral channel, integer fill) -> locality::geo::Mineral::Mix {
            return locality::geo::Mineral::Mix{static_cast<std::uint64_t>(fill)} << (static_cast<integer>(channel) * 4);
        };
        planet.emplace(
            locality::planet::Passport{
                .seed = 7,
                .radius = radius,
                .surfaceAcceleration = 3.71f,
                .orientation = quat{1.0f, 0.0f, 0.0f, 0.0f},
                .geology = {
                    .mix = nibble(Mineral::Pyroxene, 12) | nibble(Mineral::Olivine, 8) | nibble(Mineral::Feldspar, 6) | nibble(Mineral::Oxides, 11) | nibble(Mineral::Clay, 7) | nibble(Mineral::Ice, 6) | nibble(Mineral::Salts, 4) | nibble(Mineral::Iron, 5) | nibble(Mineral::Carbonaceous, 3),
                    .differentiation = 0.62f,
                    .surfaceAge = 0.78f,
                    .cohesion = 0.48f,
                    .grain = 0.55f,
                    .tectonic = 0.22f,
                    .amplitude = 200.0f,
                },
                .atmosphere = {
                    .outerRadius = radius + 350.0f,
                    .seaDensity = 0.03f,
                    .kerman = 800.0f,
                    .day = RGB{0.78f, 0.52f, 0.36f},
                },
            },
            locality::planet::Planet::recommendedDetail(radius, 8.0f));
        planet->place(context, device, Pose::from(Pos{0.0f, 0.0f, 0.0f}, HPB{0.0f, 0.0f, 0.0f}));
    }

}
