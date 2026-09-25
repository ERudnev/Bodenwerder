#include "scenarios/asterField.h"

#include <eltanin/geo/rock.q1.h>
#include <eltanin/locality/flash.q1.h>
#include <eltanin/physics/rigid.q1.h>
#include <eltanin/world.q1.h>

namespace eltanin::scenario {

    using namespace rmmr;

    void AsterField::loadResources(Writing, const rmmr::wrapper::assets::Handles&) {
    }

    void AsterField::populate(Writing context, rmmr::system::Device::Id device) {
        with<World>::placeCamera(context, Pose::from(Pos{0.0f, 35.0f, 100.0f}, HPB{0.0f, -20.0f, 0.0f}));
        // Temporary: flash spawn parked.
        //with<locality::Flash>::spawn(context, vec3{0.0f, 0.0f, 0.0f}, vec3{0.0f, 0.0f, 0.0f}, locality::Flash::Channels{.kinetic = 50.0f, .thermal = 0.0f, .brisance = 0.0f, });
        const geo::GeneralizedRecipe iceAsteroid{
            .mix = geo::GeneralizedRecipe::homogenous(0), // Ice
            .radius = 50.0f,
            .lump = 0.05f, // high sphericity, not a perfect ball
            .seed = 77,
            .spotMeters = 16.0f,
            .spotContrast = 0.5f,
        };
        const geo::GeneralizedRecipe ironAsteroid{
            .mix = geo::GeneralizedRecipe::homogenous(6), // Iron
            .radius = 16.0f, // ~¼ ice mass: iron ~8.5× denser
            .lump = 0.5f,
            .seed = 91,
            .spotMeters = 5.0f,
            .spotContrast = 0.5f,
        };
        const Pos iceOffset{0.0f, -60.0f, 0.0f};
        const Pos ironOffset{0.0f, 100.0f, 0.0f};
        const phys::rigid::CelestialGravity::Quantum iceGravity{.averageRadius = iceAsteroid.radius, .surfaceAcceleration = 2.0f};
        const phys::rigid::CelestialGravity::Quantum ironGravity{.averageRadius = ironAsteroid.radius, .surfaceAcceleration = iceGravity.surfaceAcceleration * 0.25f};
        const auto asteroid = with<geo::Rock>::spawnGenerated(context, device, Pose::from(iceOffset, HPB{0.0f, 0.0f, 0.0f}), iceAsteroid, vec3{0.0f, 0.0f, 0.0f}, vec3{0.0f, 0.0f, 0.0f});
        with<phys::rigid::CelestialGravity>::extend(context, with<geo::Rock>::get(context, asteroid).body, iceGravity);
        const auto companion = with<geo::Rock>::spawnGenerated(context, device, Pose::from(ironOffset, HPB{0.0f, 0.0f, 0.0f}), ironAsteroid, vec3{0.0f, 0.0f, 0.0f}, vec3{0.0f, 0.0f, 0.0f});
        with<phys::rigid::CelestialGravity>::extend(context, with<geo::Rock>::get(context, companion).body, ironGravity);
    }

}
