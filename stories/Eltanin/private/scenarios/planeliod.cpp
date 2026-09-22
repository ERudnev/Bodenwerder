#include "scenarios/planeliod.h"

#include "geo/celestial/sun.h"
#include <eltanin/geo/minerals.q1.h>
#include <eltanin/geo/volatiles.q1.h>
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
        using Volatile = geo::Volatile::Kind;
        auto mineral = [](Mineral channel, integer fill) -> geo::Mineral::Mix {
            return geo::Mineral::Mix{static_cast<std::uint64_t>(fill)} << (static_cast<integer>(channel) * 4);
        };
        auto volatileInventory = [](Volatile channel, integer fill) -> geo::Volatile::Mix {
            return geo::Volatile::Mix{static_cast<std::uint32_t>(fill)} << (static_cast<integer>(channel) * 4);
        };
        planet.emplace(
            planet::Passport{
                .seed = 7,
                .ageGyr = 4.54f,
                .mass = 6.4266e19,
                .radius = radius,
                .bulk = mineral(Mineral::Pyroxene, 12) | mineral(Mineral::Olivine, 8) | mineral(Mineral::Feldspar, 6) | mineral(Mineral::Oxides, 11) | mineral(Mineral::Clay, 7) | mineral(Mineral::Ice, 6) | mineral(Mineral::Salts, 4) | mineral(Mineral::Iron, 5) | mineral(Mineral::Carbonaceous, 3),
                .volatiles = volatileInventory(Volatile::Water, 6) | volatileInventory(Volatile::CarbonDioxide, 12) | volatileInventory(Volatile::Nitrogen, 2) | volatileInventory(Volatile::SulfurDioxide, 1),
                .spin = {
                    .axis = glm::normalize(vec3{0.423f, 0.906f, 0.0f}),
                    .period = 88200.0f,
                },
                .orbit = {
                    .normal = vec3{0.0f, 1.0f, 0.0f},
                    .period = 59356800.0f,
                },
                .environment = {
                    .stellarFlux = 590.0f,
                    .eccentricity = 0.0934f,
                    .tidalHeat = 0.01f,
                    .debrisFlux = 0.45f,
                },
            },
            planet::Planet::recommendedDetail(radius));
        planet->place(context, device, Pose::from(Pos{0.0f, 0.0f, 0.0f}, HPB{0.0f, 0.0f, 0.0f}));
    }

}
