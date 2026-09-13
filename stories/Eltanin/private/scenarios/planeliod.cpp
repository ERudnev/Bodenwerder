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
        with<World>::placeCamera(context, Pose::from(Pos{0.0f, 0.0f, 25000.0f}, HPB{0.0f, 0.0f, 0.0f}));
        if (const auto camera = with<World>::get_global(context).camera)
            with<controller::Camera3d>::modify(context, *camera)->moveScale = 10.0f;
        for (auto [gridId, _] : context->aspect<scene::Grid>().items()) {
            auto mesh = with<scene::actor::MeshState>::modify(context, gridId);
            mesh->scale = vec3{10.0f};
            mesh->patternScale = 1.0f;
            with<scene::Grid>::modify(context, gridId)->patternScale = 1.0f;
        }
        locality::geo::Sun::place(context, locality::geo::Sun::sol());
    }

    void Planeliod::placePlanet(Writing context, rmmr::system::Device::Id device, base::maybe<locality::planet::Planet>& planet) {
        auto nibble = [](integer channel, integer fill) -> locality::geo::Mineral::Mix {
            return locality::geo::Mineral::Mix{static_cast<std::uint64_t>(fill)} << (channel * 4);
        };
        planet.emplace(locality::planet::Passport{
            .seed = 7,
            .radius = 10000.0f,
            .surfaceAcceleration = 9.81f,
            .orientation = quat{1.0f, 0.0f, 0.0f, 0.0f},
            .geology = {
                .mix = nibble(0, 8) | nibble(1, 10) | nibble(2, 8) | nibble(6, 4),
                .differentiation = 0.35f,
                .surfaceAge = 0.7f,
                .cohesion = 0.72f,
                .grain = 0.4f,
                .tectonic = 0.015f,
                .maxRelief = 80.0f,
            },
            .atmosphere = {
                .outerRadius = 10000.0f,
                .seaDensity = 0.0f,
                .kerman = 1000.0f,
                .day = RGB{0.42f, 0.62f, 1.00f},
            },
        });
        planet->place(context, device, Pose::from(Pos{0.0f, 0.0f, 0.0f}, HPB{0.0f, 0.0f, 0.0f}));
    }

}
