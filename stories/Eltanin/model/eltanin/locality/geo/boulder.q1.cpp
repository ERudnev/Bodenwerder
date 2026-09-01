#include <eltanin/locality/geo/boulder.q1.h>

#include <eltanin/locality/geo/minerals.q1.h>
#include <eltanin/physics/body.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/resources/texture3array.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/node.q1.h>
#include <rmmr/scene/root.q1.h>

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

        constexpr float vaporAt = 4.0f;
        constexpr float cutStep = 0.5f;
        constexpr int maxCuts = 3;
        constexpr float minRadius = 0.12f;
        constexpr float bornCohesion = 0.5f;

        struct Pebble {
            vec3 center;
            float radius;
            integer seed;
        };

        auto cutCount(float cohesion) -> int {
            if (cohesion < -vaporAt)
                return -1;
            if (cohesion > 0.0f)
                return 0;
            return glm::min(maxCuts, 1 + static_cast<int>(-cohesion / cutStep));
        }

        void dichotomy(vec3 center, quat rotation, float radius, int cuts, integer seed, int axis, vector<Pebble>& out) {
            if (cuts <= 0 or radius < minRadius * 2.0f) {
                out.push_back(Pebble{.center = center, .radius = radius, .seed = seed});
                return;
            }
            const float childRadius = radius * std::cbrt(0.5f);
            vec3 unit{0.0f, 0.0f, 0.0f};
            unit[axis % 3] = 1.0f;
            const vec3 along = rotation * unit;
            const float h = radius * 0.5f;
            dichotomy(center - along * h, rotation, childRadius, cuts - 1, seed * 2 + 1, axis + 1, out);
            dichotomy(center + along * h, rotation, childRadius, cuts - 1, seed * 2 + 2, axis + 1, out);
        }

    } // namespace

    void Boulder::Actions::bindResources(Writing context) {
        if (with<Boulder>::get_global(context).resources)
            return;
        const auto material = with<resource::Assets>::find<resource::material::Asset>(context, resource::Unit::Name::from("Eltanin", "boulder"));
        if (not material) {
            context.refuse("eltanin::locality::geo::Boulder::bindResources: boulder material missing");
            return;
        }
        const auto crust = with<resource::Assets>::find<resource::texture3array::Asset>(context, resource::Unit::Name::from("Eltanin", "crust"));
        if (not crust) {
            context.refuse("eltanin::locality::geo::Boulder::bindResources: crust pack missing");
            return;
        }
        with<Boulder>::modify_global(context)->resources = Resources{.material = *material, .crust = *crust};
    }

    auto Boulder::Actions::spawnGenerated(Writing context, rmmr::system::Device::Id device, Pose pose, GeneralizedRecipe recipe, vec3 velocity, vec3 omega) -> Id {
        const auto scene = with<Thing>::get_global(context).scene;
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

        const auto& resources = with<Boulder>::get_global(context).resources;
        if (not resources)
            return context.refuse("eltanin::locality::geo::Boulder::spawnGenerated: resources not bound");
        const auto& runtimes = with<rmmr::resource::Runtimes>::get(context, device);
        if (runtimes.texture3arrays_id_mapping.find(resources->crust) == runtimes.texture3arrays_id_mapping.end()) {
            if (not with<rmmr::resource::texture3array::Asset>::install(context, resources->crust, device, generateCrust()))
                return context.refuse("eltanin::locality::geo::Boulder::spawnGenerated: crust install failed");
        }
        auto meshQuantum = with<rmmr::scene::actor::Mesh>::composeOne(context, geometryId, resources->material, resources->crust);
        if (not meshQuantum)
            return context.refuse("eltanin::locality::geo::Boulder::spawnGenerated: mesh compose failed");
        meshQuantum->spriteIndex = dominantMineral(recipe.mix);

        auto meshState = with<rmmr::scene::actor::MeshState>::defaults(RGB{1.0f, 1.0f, 1.0f}, 1.0f);
        meshState.patternScale = glm::max(0.5f, recipe.radius * 2.0f);
        meshState.heat = vec2{0.0f, 0.0f};
        const auto actor = with<rmmr::scene::Interface>::createMeshActor(context, scene, pose, std::move(*meshQuantum), meshState);

        const auto body = phys::createBody(context, phys::Body::Quantum{
            .position = dvec3{pose.position},
            .orientation = pose.rotation,
            .totalMass = mass,
            .radius = recipe.radius,
            .compound = phys::Body::Id::please_never_use_this_except_patch_rejection_mechanism(),
        }, {});
        quat prevOri = pose.rotation;
        const float omegaLen = glm::length(omega);
        if (omegaLen > 1.0e-12f) {
            const quat step = glm::angleAxis(-omegaLen * float(phys::Settings::fixedStep), omega / omegaLen);
            prevOri = glm::normalize(step * pose.rotation);
        }
        with<phys::rigid::Solid>::extend(context, body, phys::rigid::Solid::Quantum{
            .center = phys::Particle{phys::Matter{.position = dvec3{pose.position}, .mass = mass, .temperature = 0.0f, .cohesion = bornCohesion}, dvec3{pose.position} - dvec3{velocity * float(phys::Settings::fixedStep)}, vec3{0.0f, 0.0f, 0.0f}},
            .prevOri = prevOri,
            .forceAngular = vec3{0.0f, 0.0f, 0.0f},
            .kind = phys::rigid::Solid::Kind::sphere,
            .halfExtents = vec3{recipe.radius, recipe.radius, recipe.radius},
        });
        const auto thing = with<Thing>::create(context, Thing::Quantum{.bornAt = with<Thing>::get_global(context).now});
        with<Boulder>::extend(context, thing, Boulder::Quantum{.body = body, .actor = actor, .recipe = recipe});
        return thing;
    }

    void Boulder::Actions::update(Writing context) {
        vector<Id> living;
        for (auto [id, _] : context->aspect<Boulder>().items())
            living.push_back(id);
        for (const auto id : living) {
            if (not with<Boulder>::exists(context, id))
                continue;
            const auto& boulder = with<Boulder>::get(context, id);
            if (not with<phys::rigid::Solid>::exists(context, boulder.body) or not with<phys::Body>::exists(context, boulder.body) or not with<rmmr::scene::actor::Mesh>::exists(context, boulder.actor))
                continue;
            const auto& solid = with<phys::rigid::Solid>::get(context, boulder.body);
            const auto& body = with<phys::Body>::get(context, boulder.body);
            const int cuts = cutCount(solid.center.cohesion);
            if (cuts == 0)
                continue;
            if (cuts > 0 and boulder.recipe.radius >= minRadius * 2.0f) {
                if (with<rmmr::scene::actor::Mesh>::exists(context, boulder.actor)) {
                    const auto device = with<rmmr::scene::actor::Mesh>::get(context, boulder.actor).device;
                    const vec3 linear = vec3{(body.position - solid.center.prev) / phys::Settings::fixedStep};
                    vector<Pebble> pieces;
                    dichotomy(vec3{body.position}, body.orientation, boulder.recipe.radius, cuts, boulder.recipe.seed, 0, pieces);
                    if (pieces.size() >= 2) {
                        for (const Pebble& piece : pieces) {
                            GeneralizedRecipe child = boulder.recipe;
                            child.radius = piece.radius;
                            child.seed = piece.seed;
                            child.spotMeters = piece.radius * 2.0f;
                            spawnGenerated(context, device, Pose{.position = piece.center, .rotation = body.orientation}, child, linear, vec3{0.0f, 0.0f, 0.0f});
                        }
                    }
                }
            }
            with<Boulder>::kraken(context, id);
        }
    }

    void Boulder::Actions::radiate(Stewarding context, seconds dt) {
        if (dt <= 0)
            return;
        const float sigma = phys::Settings::radiateSigma;
        const float sky = with<rmmr::scene::Root>::get(context, with<Thing>::get_global(context).scene).atmosphereTemperature;
        auto solids = context.direct<phys::rigid::Solid>();
        for (auto [_, boulder] : context.direct<Boulder>().items) {
            auto* solid = solids.items.find(boulder.body);
            if (not solid)
                continue;
            auto& particle = solid->center;
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

    void Boulder::Actions::followBody(Stewarding context) {
        auto nodes = context.direct<rmmr::scene::Node>();
        auto bodies = context.direct<phys::Body>();
        for (auto [_, boulder] : context->aspect<Boulder>().items()) {
            auto* node = nodes.items.find(boulder.actor);
            if (not node)
                continue;
            auto* body = bodies.items.find(boulder.body);
            if (not body)
                continue;
            node->pose = body->pose();
        }
    }

    auto Boulder::customAspectReactions() -> const Behavior {
        return {
            reaction::structural::custody<Boulder, rmmr::scene::actor::Mesh, &Boulder::Quantum::actor>{},
            reaction::structural::custody<Boulder, phys::rigid::Solid, &Boulder::Quantum::body>{},
        };
    }

}
