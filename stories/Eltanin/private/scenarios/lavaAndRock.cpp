#include "scenarios/lavaAndRock.h"

#include <eltanin/locality/geo/boulder.q1.h>
#include <eltanin/physics/body.q1.h>
#include <eltanin/physics/rigid.q1.h>
#include <eltanin/world.q1.h>

namespace eltanin::scenario {

    using namespace rmmr;

    void LavaAndRock::loadResources(Writing, const rmmr::wrapper::assets::Handles&) {
    }

    void LavaAndRock::populate(Writing context, rmmr::system::Device::Id device) {
        with<World>::placeCamera(context, Pose::from(Pos{0.0f, 35.0f, 100.0f}, HPB{0.0f, -20.0f, 0.0f}));
        const auto brick = with<locality::geo::Rock>::spawnLavaBrick(context, device, Pose::from(Pos{0.0f, 0.0f, 0.0f}, HPB{0.0f, 0.0f, 0.0f}));
        rocks.push_back(brick);
        constexpr float brickKelvin = 1800.0f;
        {
            const auto& rock = with<locality::geo::Rock>::get(context, brick);
            auto crystal = with<phys::rigid::Crystal>::modify(context, rock.body);
            for (phys::Particle& particle : crystal->particles)
                particle.temperature = brickKelvin;
            crystal->refreshMatter(*with<phys::Body>::modify(context, rock.body));
            with<rmmr::scene::actor::MeshState>::modify(context, rock.actor)->heat.x = brickKelvin;
        }

        constexpr int count = 50;
        constexpr int rows = 16;
        constexpr float diameter = 0.5f;
        constexpr float spacing = 1.0f;
        constexpr float rowStep = 2.0f;
        constexpr float kelvinMax = 2000.0f;
        boulders.reserve(static_cast<std::size_t>(count) * rows);
        for (int row = 0; row < rows; ++row) {
            for (int index = 0; index < count; ++index) {
                const float kelvin = kelvinMax * static_cast<float>(index) / static_cast<float>(count - 1);
                const float x = 55.0f + spacing * static_cast<float>(index);
                const float z = 70.0f + rowStep * static_cast<float>(row);
                const locality::geo::GeneralizedRecipe recipe{
                    .mix = locality::geo::GeneralizedRecipe::homogenous(row),
                    .radius = diameter * 0.5f,
                    .lump = 0.55f,
                    .seed = 7 + index + row * 1000,
                    .spotMeters = diameter,
                    .spotContrast = 0.0f,
                };
                const auto id = with<locality::geo::Boulder>::spawnGenerated(context, device, Pose::from(Pos{x, 0.0f, z}, HPB{0.0f, 0.0f, 0.0f}), recipe, vec3{0.0f, 0.0f, 0.0f}, vec3{0.0f, 0.0f, 0.0f});
                boulders.push_back(id);
                const auto& boulder = with<locality::geo::Boulder>::get(context, id);
                with<phys::rigid::Solid>::modify(context, boulder.body)->center.temperature = kelvin;
                with<rmmr::scene::actor::MeshState>::modify(context, boulder.actor)->heat.x = kelvin;
            }
        }
    }

}
