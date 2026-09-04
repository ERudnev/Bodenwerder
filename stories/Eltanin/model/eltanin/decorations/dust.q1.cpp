#include <eltanin/decorations/dust.q1.h>

#include <eltanin/locality/thing.q1.h>
#include "physics/settings.h"
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/meshpack.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/resources/texpack.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/node.q1.h>
#include <rmmr/scene/root.q1.h>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>

#include <random>
#include <span>

namespace eltanin::decorations {

    using namespace fqsm::api;
    using rmmr::Pose;
    using rmmr::RGB;
    using rmmr::vec3;

    namespace {

        constexpr float dustLife = 10.0f;
        constexpr float dustLifeJitter = 5.0f;
        constexpr float dustDensity = 7850.0f; // kg/m³; blast coupling only
        constexpr seconds dustCool = 4; // toward sky; Flash soak can raise T again

        auto massOf(vec3 half) -> float {
            const vec3 size = glm::max(half, vec3{0.08f, 0.08f, 0.08f});
            return 8.0f * size.x * size.y * size.z * dustDensity;
        }

        auto lifeBorn() -> seconds {
            static thread_local std::mt19937 rng{std::random_device{}()};
            std::uniform_real_distribution<float> jitter{-dustLifeJitter, dustLifeJitter};
            return seconds{glm::max(1.0f, dustLife + jitter(rng))};
        }

        void paintKeep(Writing context, rmmr::scene::actor::Mesh::Id actor, float cohesion, float lifeRemaining) {
            if (with<rmmr::scene::actor::MeshState>::exists(context, actor))
                with<rmmr::scene::actor::MeshState>::modify(context, actor)->opacity = glm::clamp(cohesion, 0.0f, 1.0f) * glm::clamp(lifeRemaining, 0.0f, 1.0f);
        }

        void paintHeats(Writing context, rmmr::scene::actor::Mesh::Id actor, phys::Kelvins temperature) {
            if (with<rmmr::scene::actor::MeshState>::exists(context, actor))
                with<rmmr::scene::actor::MeshState>::modify(context, actor)->heat.x = temperature;
            if (not with<rmmr::scene::actor::Mesh>::exists(context, actor))
                return;
            const auto count = with<rmmr::scene::actor::Mesh>::get(context, actor).instanceCount;
            if (count <= 0)
                return;
            vector<float> heats(static_cast<std::size_t>(count), temperature);
            with<rmmr::scene::actor::Mesh>::writeHeats(context, actor, std::span<const float>{heats});
        }

        void paint(Writing context, rmmr::scene::actor::Mesh::Id actor, float cohesion, float lifeRemaining, phys::Kelvins temperature) {
            paintKeep(context, actor, cohesion, lifeRemaining);
            paintHeats(context, actor, temperature);
        }

        auto hang(Writing context, rmmr::scene::actor::Mesh::Id actor, vec3 linear, vec3 omega, phys::Kelvins temperature, float cohesion, vec3 half, float mass) -> Dust::Id {
            if (not with<rmmr::scene::Node>::exists(context, actor))
                return actor;
            const auto life = lifeBorn();
            paint(context, actor, cohesion, 1.0f, temperature);
            with<Dust>::extend(context, actor, Dust::Quantum{.linear = linear, .omega = omega, .temperature = temperature, .cohesion = cohesion, .half = half, .mass = mass, .life = life, .lifeBorn = life});
            return actor;
        }

        auto wreckSurfaces(Reading context, rmmr::resource::geometry::Asset::Id geometryId, rmmr::resource::material::Asset::Id wreck) -> umap<rmmr::resource::geometry::SurfaceId, rmmr::resource::material::Instance> {
            umap<rmmr::resource::geometry::SurfaceId, rmmr::resource::material::Instance> surfaces;
            const auto& geometry = with<rmmr::resource::geometry::Asset>::get(context, geometryId);
            if (geometry.entries.empty() or geometry.surfaceCatalogs.empty())
                return surfaces;
            const auto& catalog = geometry.surfaceCatalogs.front();
            for (const auto& [_, surface] : catalog)
                surfaces.emplace(surface, rmmr::resource::material::Instance{.material = wreck, .textures = {{"albedoMap", "wreckage_experimental.jpg"}}});
            return surfaces;
        }

        auto composeBox(Writing context, Pose pose, vec3 half) -> rmmr::scene::actor::Mesh::Id {
            const auto scene = with<locality::Thing>::get_global(context).scene;
            const auto& resources = with<Dust>::get_global(context).resources;
            if (not resources)
                return context.refuse("eltanin::decorations::Dust::spawn: resources not bound");
            auto surfaces = wreckSurfaces(context, resources->scrap, resources->wreck);
            if (surfaces.empty())
                return context.refuse("eltanin::decorations::Dust::spawn: scrap has no entry");
            auto meshQuantum = with<rmmr::scene::actor::Mesh>::compose(context, rmmr::resource::meshpack::Asset::Resolved{.geometry = resources->scrap, .entry = rmmr::resource::geometry::EntryId{0}, .surfaces = std::move(surfaces), .texpack = resources->mech});
            if (not meshQuantum)
                return context.refuse("eltanin::decorations::Dust::spawn: mesh compose failed");
            const vec3 scale{half.x * 2.0f, half.y * 2.0f, half.z * 2.0f};
            return with<rmmr::scene::Interface>::createMeshActor(context, scene, pose, std::move(*meshQuantum), with<rmmr::scene::actor::MeshState>::defaults(RGB{1.0f, 1.0f, 1.0f}, 1.0f, scale));
        }

        void dropActor(Writing context, Dust::Id id) {
            const auto scene = with<locality::Thing>::get_global(context).scene;
            if (with<rmmr::scene::Node_group>::exists(context, scene) and with<rmmr::scene::Node_group>::get(context, scene).contains(id))
                with<rmmr::scene::Node_group>::deleteElement(context, scene, id);
            else if (with<rmmr::scene::Node>::exists(context, id))
                with<rmmr::scene::Node>::remove(context, id);
        }

        auto skyKelvin(Reading context) -> float {
            return with<rmmr::scene::Root>::get(context, with<locality::Thing>::get_global(context).scene).atmosphereTemperature;
        }

    }

    void Dust::Actions::bindResources(Writing context) {
        if (with<Dust>::get_global(context).resources)
            return;
        const auto scrap = with<rmmr::resource::Assets>::find<rmmr::resource::geometry::Asset>(context, rmmr::resource::Unit::Name::from("Eltanin", "scrap"));
        if (not scrap) {
            context.refuse("eltanin::decorations::Dust::bindResources: scrap geometry missing");
            return;
        }
        const auto wreck = with<rmmr::resource::Assets>::find<rmmr::resource::material::Asset>(context, rmmr::resource::Unit::Name::from("Eltanin", "dustWreck"));
        if (not wreck) {
            context.refuse("eltanin::decorations::Dust::bindResources: dustWreck material missing");
            return;
        }
        const auto mech = with<rmmr::resource::Assets>::find<rmmr::resource::texpack::Pack>(context, rmmr::resource::Unit::Name::from("Eltanin", "mech"));
        if (not mech) {
            context.refuse("eltanin::decorations::Dust::bindResources: mech texpack missing");
            return;
        }
        with<Dust>::modify_global(context)->resources = Resources{.scrap = *scrap, .wreck = *wreck, .mech = *mech};
    }

    void Dust::Actions::update(Writing context, seconds dt) {
        if (dt <= 0)
            return;
        const float sky = skyKelvin(context);
        const float remainingCool = glm::max(0.0f, 1.0f - float(dt) / float(dustCool));
        const float cool = remainingCool * remainingCool;
        vector<Id> living;
        for (auto [id, _] : context->aspect<Dust>().items())
            living.push_back(id);
        vector<Id> gone;
        for (const auto id : living) {
            if (not with<Dust>::exists(context, id) or not with<rmmr::scene::Node>::exists(context, id)) {
                gone.push_back(id);
                continue;
            }
            auto dust = with<Dust>::modify(context, id);
            auto node = with<rmmr::scene::Node>::modify(context, id);
            node->pose.position += dust->linear * float(dt);
            const float spin = glm::length(dust->omega);
            if (spin > 1.0e-8f)
                node->pose.rotation = glm::normalize(glm::angleAxis(spin * float(dt), dust->omega / spin) * node->pose.rotation);
            dust->life -= dt;
            if (dust->temperature > sky)
                dust->temperature = glm::max(sky, dust->temperature * cool);
            const float lifeRemaining = dust->lifeBorn > seconds{} ? glm::clamp(float(dust->life / dust->lifeBorn), 0.0f, 1.0f) : 0.0f;
            paint(context, id, dust->cohesion, lifeRemaining, dust->temperature);
            if (dust->life <= 0 or dust->cohesion <= 0.0f)
                gone.push_back(id);
        }
        for (const auto id : gone)
            dropActor(context, id);
    }

    auto Dust::Actions::spawn(Writing context, Pose pose, vec3 half, vec3 linear, vec3 omega, phys::Kelvins temperature, float cohesion) -> Id {
        const vec3 size = glm::max(half, vec3{0.08f, 0.08f, 0.08f});
        return hang(context, composeBox(context, pose, size), linear, omega, temperature, glm::clamp(cohesion, 0.0f, 1.0f), size, massOf(size));
    }

    auto Dust::Actions::spawnMesh(Writing context, Pose pose, vector<rmmr::scene::actor::Mesh::Occurrence> occurrences, vec3 linear, vec3 omega, phys::Kelvins temperature, float cohesion, vec3 half, float latticeStep) -> Id {
        if (occurrences.empty())
            return context.refuse("eltanin::decorations::Dust::spawnMesh: no occurrences");
        const auto& resources = with<Dust>::get_global(context).resources;
        if (not resources)
            return context.refuse("eltanin::decorations::Dust::spawnMesh: resources not bound");
        for (auto& occurrence : occurrences) {
            occurrence.entry.texpack = resources->mech;
            for (auto& [_, instance] : occurrence.entry.surfaces) {
                instance.material = resources->wreck;
                instance.textures = {{"albedoMap", "wreckage_experimental.jpg"}};
            }
        }
        auto meshQuantum = with<rmmr::scene::actor::Mesh>::compose(context, occurrences);
        if (not meshQuantum)
            return context.refuse("eltanin::decorations::Dust::spawnMesh: mesh compose failed");
        const auto scene = with<locality::Thing>::get_global(context).scene;
        auto look = with<rmmr::scene::actor::MeshState>::defaults(RGB{1.0f, 1.0f, 1.0f}, 1.0f);
        look.latticeStep = latticeStep;
        look.heat.x = temperature;
        const auto actor = with<rmmr::scene::Interface>::createMeshActor(context, scene, pose, std::move(*meshQuantum), std::move(look));
        const vec3 size = glm::max(half, vec3{0.08f, 0.08f, 0.08f});
        return hang(context, actor, linear, omega, temperature, glm::clamp(cohesion, 0.0f, 1.0f), size, massOf(size));
    }

}
