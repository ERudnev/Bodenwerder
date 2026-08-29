#include <eltanin/locality/geo/boulder.q1.h>

#include <eltanin/locality/geo/minerals.q1.h>
#include <eltanin/physics/compound.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/resources/texture3array.q1.h>
#include <rmmr/scene/node.q1.h>

#include "physics/system.h"
#include "geo/stones/boulderMesh.h"
#include "geo/stones/crust.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cmath>
#include <numbers>

namespace eltanin::locality::geo {

    using namespace fqsm::api;
    using namespace rmmr;

    namespace {

        constexpr int mixChannels = 16;

        auto mixDensity(Mix mix) -> float {
            if (mix == 0)
                return 0.0f;
            const auto& table = Mineral::table();
            float density = 0.0f;
            const auto channels = table.size() < static_cast<std::size_t>(mixChannels) ? table.size() : static_cast<std::size_t>(mixChannels);
            for (std::size_t channel = 0; channel < channels; ++channel) {
                const float fill = static_cast<float>((mix >> (channel * 4)) & 0xF) / 15.0f;
                density += fill * table[channel].kgPerCubicMeter();
            }
            return density;
        }

        auto dominantMineral(Mix mix) -> integer {
            integer dominant = 0;
            Mix weight = 0;
            for (integer channel = 0; channel < mixChannels; ++channel) {
                const Mix candidate = (mix >> (channel * 4)) & 0xF;
                if (candidate > weight) {
                    dominant = channel;
                    weight = candidate;
                }
            }
            return dominant;
        }

        auto sphereArea(float mass) -> float {
            const float volume = glm::max(mass / 1000.0f, 1.0e-6f); // kg → the g/cm³·m³ quantity this formula was written for
            const float radius = std::cbrt((3.0f * volume) / (4.0f * std::numbers::pi_v<float>));
            return 4.0f * std::numbers::pi_v<float> * radius * radius;
        }

    } // namespace

    auto Boulder::Actions::spawnGenerated(Writing context, rmmr::scene::Root::Id root, rmmr::system::Device::Id device, Pose pose, GeneralizedRecipe recipe, vec3 velocity, vec3 omega) -> Id {
        if (recipe.radius <= 0.0f)
            return context.refuse("eltanin::locality::geo::Boulder::spawnGenerated: radius must be positive");
        if (recipe.mix == 0)
            return context.refuse("eltanin::locality::geo::Boulder::spawnGenerated: mix is vacuum");

        auto cpu = meshDebris(recipe);
        if (cpu.positions.empty())
            return context.refuse("eltanin::locality::geo::Boulder::spawnGenerated: no surface");
        const float volume = (4.0f / 3.0f) * std::numbers::pi_v<float> * recipe.radius * recipe.radius * recipe.radius;
        const float mass = volume * mixDensity(recipe.mix);
        if (mass <= 0.0f)
            return context.refuse("eltanin::locality::geo::Boulder::spawnGenerated: mass must be positive");

        const auto manager = with<rmmr::resource::Manager>::singleton(context);
        const auto geometryId = with<rmmr::resource::Unit_group>::addElement(context, manager, rmmr::resource::Unit::Quantum{.name = rmmr::resource::Unit::Name::from("Eltanin", "rock")});
        with<rmmr::resource::geometry::Asset>::extend(context, geometryId, rmmr::resource::geometry::Asset::Quantum{});
        if (not with<rmmr::resource::geometry::Asset>::install(context, geometryId, device, cpu))
            return context.refuse("eltanin::locality::geo::Boulder::spawnGenerated: geometry install failed");

        const auto materialName = rmmr::resource::Unit::Name::from("Eltanin", "boulder");
        const auto boulderMaterial = with<rmmr::resource::Assets>::find<rmmr::resource::material::Asset>(context, materialName);
        if (not boulderMaterial)
            return context.refuse("eltanin::locality::geo::Boulder::spawnGenerated: boulder material missing");
        const auto crust = with<rmmr::resource::Assets>::find<rmmr::resource::texture3array::Asset>(context, rmmr::resource::Unit::Name::from("Eltanin", "crust"));
        if (not crust)
            return context.refuse("eltanin::locality::geo::Boulder::spawnGenerated: crust pack missing");
        const auto& runtimes = with<rmmr::resource::Runtimes>::get(context, device);
        if (runtimes.texture3arrays_id_mapping.find(*crust) == runtimes.texture3arrays_id_mapping.end()) {
            if (not with<rmmr::resource::texture3array::Asset>::install(context, *crust, device, generateCrust()))
                return context.refuse("eltanin::locality::geo::Boulder::spawnGenerated: crust install failed");
        }
        auto meshQuantum = with<rmmr::scene::actor::Mesh>::composeOne(context, geometryId, *boulderMaterial, *crust);
        if (not meshQuantum)
            return context.refuse("eltanin::locality::geo::Boulder::spawnGenerated: mesh compose failed");
        meshQuantum->spriteIndex = dominantMineral(recipe.mix);

        auto meshState = with<rmmr::scene::actor::MeshState>::defaults(RGB{1.0f, 1.0f, 1.0f}, 1.0f);
        meshState.patternScale = glm::max(0.5f, recipe.radius * 2.0f);
        meshState.heat = vec2{0.0f, 0.0f};
        const auto actor = with<rmmr::scene::Interface>::createMeshActor(context, root, pose, std::move(*meshQuantum), meshState);

        const auto body = with<phys::Body>::create(context, phys::Body::Quantum{
            .position = dvec3{pose.position},
            .orientation = pose.rotation,
            .totalMass = mass,
            .radius = recipe.radius,
        });
        quat prevOri = pose.rotation;
        const float omegaLen = glm::length(omega);
        if (omegaLen > 1.0e-12f) {
            const quat step = glm::angleAxis(-omegaLen * phys::Particle::dt, omega / omegaLen);
            prevOri = glm::normalize(step * pose.rotation);
        }
        with<phys::rigid::Ball>::extend(context, body, phys::rigid::Ball::Quantum{
            .center = phys::Particle{phys::Matter{.position = dvec3{pose.position}, .mass = mass, .temperature = 0.0f, .cohesion = 0.0f}, dvec3{pose.position} - dvec3{velocity * phys::Particle::dt}, vec3{0.0f, 0.0f, 0.0f}},
            .prevOri = prevOri,
            .forceAngular = vec3{0.0f, 0.0f, 0.0f},
        });
        with<phys::Compound>::extend(context, body, phys::Compound::Quantum{.members = {}});
        const auto thing = with<Thing>::create(context, Thing::Quantum{.bornAt = with<Thing>::get_global(context).now});
        with<Boulder>::extend(context, thing, Boulder::Quantum{.body = body, .actor = actor});
        return thing;
    }

    void Boulder::Actions::update(Stewarding) {
    }

    void Boulder::Actions::radiate(Stewarding context, float dt) {
        if (dt <= 0.0f)
            return;
        const float sigma = phys::Settings::radiateSigma;
        const float sky = phys::Settings::skyKelvin;
        auto balls = context.direct<phys::rigid::Ball>();
        for (auto [_, boulder] : context.direct<Boulder>().items) {
            auto* ball = balls.items.find(boulder.body);
            if (not ball)
                continue;
            auto& particle = ball->center;
            if (particle.mass <= 0.0f)
                continue;
            const float temperature = glm::max(particle.temperature, sky);
            const float t2 = temperature * temperature;
            const float lost = sigma * sphereArea(particle.mass) * t2 * t2 * dt;
            const float energy = particle.mass * temperature;
            particle.temperature = glm::max(sky, (energy - lost) / particle.mass);
            if (not with<rmmr::scene::actor::MeshState>::exists(context, boulder.actor))
                continue;
            const vec2 nextHeat{particle.temperature, 0.0f};
            if (with<rmmr::scene::actor::MeshState>::get(context, boulder.actor).heat == nextHeat)
                continue;
            with<rmmr::scene::actor::MeshState>::modify(context, boulder.actor)->heat = nextHeat;
        }
    }

    struct Boulder::Internals : Boulder::DefaultInternals {
        static void followBody(Reacting context) {
            using namespace api_for_internals;
            for (auto [id, boulder] : context.proposal.aspect<Boulder>().items()) {
                if (not my::ward(context, id, &Quantum::actor)) { my::remove(context, id); continue; }
                if (not with<rmmr::scene::Node>::exists(context, boulder.actor)) { my::remove(context, id); continue; }
                const auto* body = my::ward(context, id, &Quantum::body);
                if (not body) { my::remove(context, id); continue; }
                with<rmmr::scene::Node>::modify(context, boulder.actor)->pose = body->pose();
            }
        }
    };

    auto Boulder::customAspectReactions() -> const Behavior {
        return {
            reaction::structural::custody<Boulder, rmmr::scene::actor::Mesh, &Boulder::Quantum::actor>{},
            reaction::structural::custody<Boulder, phys::rigid::Ball, &Boulder::Quantum::body>{},
            reaction::aspect_wide<Boulder, phys::Body>(&Boulder::Internals::followBody),
        };
    }

}
