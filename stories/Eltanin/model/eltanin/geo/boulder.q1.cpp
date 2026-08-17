#include <eltanin/geo/boulder.q1.h>

#include <eltanin/geo/minerals.q1.h>
#include <eltanin/physics/particle.q1.h>
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/resources/texture3array.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/node.q1.h>
#include <rmmr/scene/root.q1.h>

#include "physics/system.h"
#include "stones/boulderMesh.h"
#include "stones/crust.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <numbers>

namespace eltanin::geo {

    using namespace fqsm::api;
    using namespace rmmr;

    namespace {

        constexpr float diameterMin = 0.5f;
        constexpr float diameterMax = 10.0f;
        constexpr std::size_t clastCount = 6;

        auto restOffset(float restRadius, std::size_t index) -> vec3 {
            switch (index) {
            case 0: return vec3{restRadius, 0.0f, 0.0f};
            case 1: return vec3{-restRadius, 0.0f, 0.0f};
            case 2: return vec3{0.0f, restRadius, 0.0f};
            case 3: return vec3{0.0f, -restRadius, 0.0f};
            case 4: return vec3{0.0f, 0.0f, restRadius};
            default: return vec3{0.0f, 0.0f, -restRadius};
            }
        }

        auto thermalMass(integer mineral, float diameterMeters) -> float {
            const float radius = diameterMeters * 0.5f;
            const float volume = (4.0f / 3.0f) * std::numbers::pi_v<float> * radius * radius * radius;
            return Mineral::table()[static_cast<std::size_t>(mineral)].density * volume;
        }

    } // namespace

    auto Boulder::Actions::spawn(Writing context, rmmr::scene::Root::Id root, rmmr::system::Device::Id device, Pose pose, Recipe recipe, vec3 velocity, vec3 omega) -> Id {
        if (recipe.mineral < 0 or recipe.mineral >= static_cast<integer>(Mineral::table().size()))
            return context.refuse("eltanin::geo::Boulder::spawn: mineral out of range");
        if (recipe.diameterMeters < diameterMin or recipe.diameterMeters > diameterMax)
            return context.refuse("eltanin::geo::Boulder::spawn: diameterMeters out of range");

        auto cpu = meshBoulder(recipe);
        if (cpu.positions.empty())
            return context.refuse("eltanin::geo::Boulder::spawn: no surface");

        const auto manager = with<rmmr::resource::Manager>::singleton(context);
        if (not manager)
            return context.refuse("eltanin::geo::Boulder::spawn: resource Manager missing");
        if (not with<rmmr::resource::Unit_group>::exists(context, *manager))
            with<rmmr::resource::Unit_group>::extend(context, *manager);
        const auto geometryId = with<rmmr::resource::Unit_group>::addElement(context, *manager, rmmr::resource::Unit::Quantum{.name = rmmr::resource::Unit::Name::from("Eltanin", "boulder")});
        with<rmmr::resource::geometry::Asset>::extend(context, geometryId, rmmr::resource::geometry::Asset::Quantum{});
        if (not with<rmmr::resource::geometry::Asset>::install(context, geometryId, device, cpu))
            return context.refuse("eltanin::geo::Boulder::spawn: geometry install failed");

        const auto boulderMaterial = with<rmmr::resource::Assets>::find<rmmr::resource::material::Asset>(context, rmmr::resource::Unit::Name::from("Eltanin", "boulder"));
        if (not boulderMaterial)
            return context.refuse("eltanin::geo::Boulder::spawn: boulder material missing");
        const auto crust = with<rmmr::resource::Assets>::find<rmmr::resource::texture3array::Asset>(context, rmmr::resource::Unit::Name::from("Eltanin", "crust"));
        if (not crust)
            return context.refuse("eltanin::geo::Boulder::spawn: crust pack missing");
        const auto& runtimes = with<rmmr::resource::Runtimes>::get(context, device);
        if (runtimes.texture3arrays_id_mapping.find(*crust) == runtimes.texture3arrays_id_mapping.end()) {
            if (not with<rmmr::resource::texture3array::Asset>::install(context, *crust, device, generateCrust()))
                return context.refuse("eltanin::geo::Boulder::spawn: crust install failed");
        }
        auto meshQuantum = with<rmmr::scene::actor::Mesh>::composeOne(context, geometryId, *boulderMaterial, *crust);
        if (not meshQuantum)
            return context.refuse("eltanin::geo::Boulder::spawn: mesh compose failed");
        meshQuantum->spriteIndex = recipe.mineral;

        const float restRadius = recipe.diameterMeters * 0.5f;
        const float volume = (4.0f / 3.0f) * std::numbers::pi_v<float> * restRadius * restRadius * restRadius;
        const float massEach = Mineral::table()[static_cast<std::size_t>(recipe.mineral)].density * volume / static_cast<float>(clastCount);
        const float kelvin = glm::max(recipe.kelvin, 0.0f);
        const float erosion = glm::clamp(recipe.erosion, 0.0f, 1.0f);

        const auto actor = with<rmmr::scene::Interface>::createMeshActor(context, root, pose, std::move(*meshQuantum), rmmr::scene::actor::MeshState::Quantum{.albedo = RGB{1.0f, 1.0f, 1.0f}, .scale = vec3{1.0f, 1.0f, 1.0f}, .latticeStep = 1.0f, .patternScale = recipe.diameterMeters, .opacity = 1.0f, .visible = true, .heat = vec2{kelvin, erosion}});

        vector<phys::Particle::Id> ids;
        ids.reserve(clastCount);
        for (std::size_t index = 0; index < clastCount; ++index) {
            const vec3 local = restOffset(restRadius, index);
            const vec3 world = pose.position + pose.rotation * local;
            const vec3 spin = glm::cross(omega, pose.rotation * local);
            ids.push_back(with<phys::Particle>::create(context, phys::Particle::Quantum{.current = world, .prev = world - (velocity + spin) * phys::Settings::fixedDtS, .mass = massEach}));
        }

        const auto body = with<phys::Clast>::create(context, phys::Clast::Quantum{
            .particles = std::move(ids),
            .restRadius = restRadius,
            .restored = pose,
        });
        return with<Boulder>::create(context, Boulder::Quantum{.body = body, .actor = actor, .mineral = recipe.mineral, .diameterMeters = recipe.diameterMeters, .kelvin = kelvin, .erosion = erosion});
    }

    void Boulder::Actions::syncPose(Stewarding context) {
        auto clasts = context.direct<phys::Clast>();
        for (auto [_, boulder] : context.direct<Boulder>().items) {
            auto* clast = clasts.items.find(boulder.body);
            if (not clast)
                continue;
            if (not with<rmmr::scene::Node>::exists(context, boulder.actor))
                continue;
            if (not clast->restored.near(with<rmmr::scene::Node>::get(context, boulder.actor).pose))
                with<rmmr::scene::Node>::modify(context, boulder.actor)->pose = clast->restored;
        }
    }

    void Boulder::Actions::radiate(Stewarding context, float dt) {
        if (dt <= 0.0f)
            return;
        const float sigma = phys::Settings::radiateSigma;
        const float sky = phys::Settings::skyKelvin;
        for (auto [_, boulder] : context.direct<Boulder>().items) {
            const float mass = thermalMass(boulder.mineral, boulder.diameterMeters);
            if (mass <= 0.0f)
                continue;
            const float radius = boulder.diameterMeters * 0.5f;
            const float area = 4.0f * std::numbers::pi_v<float> * radius * radius;
            const float kelvin = glm::max(boulder.kelvin, sky);
            const float t2 = kelvin * kelvin;
            const float lost = sigma * area * t2 * t2 * dt;
            const float energy = mass * kelvin;
            boulder.kelvin = glm::max(sky, (energy - lost) / mass);
            if (boulder.kelvin >= Mineral::table()[static_cast<std::size_t>(boulder.mineral)].glowKelvin)
                boulder.erosion = 0.0f;
            if (not with<rmmr::scene::actor::MeshState>::exists(context, boulder.actor))
                continue;
            const vec2 nextHeat{boulder.kelvin, boulder.erosion};
            if (with<rmmr::scene::actor::MeshState>::get(context, boulder.actor).heat == nextHeat)
                continue;
            with<rmmr::scene::actor::MeshState>::modify(context, boulder.actor)->heat = nextHeat;
        }
    }

    auto Boulder::customAspectReactions() -> const Behavior {
        return {
            reaction::structural::custody<Boulder, phys::Clast, &Boulder::Quantum::body>{},
            reaction::structural::custody<Boulder, rmmr::scene::actor::Mesh, &Boulder::Quantum::actor>{},
        };
    }

}
