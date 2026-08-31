#include <eltanin/decorations/dust.q1.h>

#include <eltanin/locality/thing.q1.h>
#include "physics/settings.h"
#include <rmmr/resources/geometry.q1.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/materials.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/scene/actors/mesh.q1.h>
#include <rmmr/scene/node.q1.h>
#include <rmmr/scene/root.q1.h>

#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>

namespace eltanin::decorations {

    using namespace fqsm::api;
    using rmmr::Pose;
    using rmmr::RGB;
    using rmmr::vec3;

    namespace {

        constexpr seconds bornLife = 4;

        void paintHeat(Writing context, rmmr::scene::actor::Mesh::Id actor, phys::Kelvins temperature) {
            if (with<rmmr::scene::actor::MeshState>::exists(context, actor))
                with<rmmr::scene::actor::MeshState>::modify(context, actor)->heat.x = temperature;
        }

        auto hang(Writing context, rmmr::scene::actor::Mesh::Id actor, vec3 linear, vec3 omega, phys::Kelvins temperature, vec3 half) -> Dust::Id {
            if (not with<rmmr::scene::Node>::exists(context, actor))
                return actor;
            paintHeat(context, actor, temperature);
            with<Dust>::extend(context, actor, Dust::Quantum{.linear = linear, .omega = omega, .temperature = temperature, .bornTemperature = temperature, .half = half, .life = bornLife});
            return actor;
        }

        auto composeBox(Writing context, Pose pose, vec3 half) -> rmmr::scene::actor::Mesh::Id {
            const auto scene = with<locality::Thing>::get_global(context).scene;
            const auto& resources = with<Dust>::get_global(context).resources;
            if (not resources)
                return context.refuse("eltanin::decorations::Dust::spawn: resources not bound");
            auto meshQuantum = with<rmmr::scene::actor::Mesh>::composeOne(context, resources->scrap, resources->glow);
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

    }

    void Dust::Actions::bindResources(Writing context) {
        if (with<Dust>::get_global(context).resources)
            return;
        const auto scrap = with<rmmr::resource::Assets>::find<rmmr::resource::geometry::Asset>(context, rmmr::resource::Unit::Name::from("Eltanin", "scrap"));
        if (not scrap) {
            context.refuse("eltanin::decorations::Dust::bindResources: scrap geometry missing");
            return;
        }
        const auto glow = with<rmmr::resource::Assets>::find<rmmr::resource::material::Asset>(context, rmmr::resource::Unit::Name::from("Eltanin", "dust"));
        if (not glow) {
            context.refuse("eltanin::decorations::Dust::bindResources: dust material missing");
            return;
        }
        with<Dust>::modify_global(context)->resources = Resources{.scrap = *scrap, .glow = *glow};
    }

    void Dust::Actions::update(Writing context, seconds dt) {
        if (dt <= 0)
            return;
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
            const float remaining = dust->life > seconds{} ? float(dust->life / bornLife) : 0.0f;
            dust->temperature = dust->bornTemperature * remaining * remaining;
            paintHeat(context, id, dust->temperature);
            if (dust->life <= 0)
                gone.push_back(id);
        }
        for (const auto id : gone)
            dropActor(context, id);
    }

    auto Dust::Actions::spawn(Writing context, Pose pose, vec3 half, vec3 linear, vec3 omega, phys::Kelvins temperature) -> Id {
        const vec3 size = glm::max(half, vec3{0.08f, 0.08f, 0.08f});
        return hang(context, composeBox(context, pose, size), linear, omega, temperature, size);
    }

    auto Dust::Actions::spawnMesh(Writing context, Pose pose, vector<rmmr::scene::actor::Mesh::Occurrence> occurrences, vec3 linear, vec3 omega, phys::Kelvins temperature, vec3 half, float latticeStep) -> Id {
        if (occurrences.empty())
            return context.refuse("eltanin::decorations::Dust::spawnMesh: no occurrences");
        const auto& resources = with<Dust>::get_global(context).resources;
        if (not resources)
            return context.refuse("eltanin::decorations::Dust::spawnMesh: resources not bound");
        for (auto& occurrence : occurrences) {
            occurrence.entry.texpack = {};
            for (auto& [_, instance] : occurrence.entry.surfaces) {
                instance.material = resources->glow;
                instance.textures = {};
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
        return hang(context, actor, linear, omega, temperature, glm::max(half, vec3{0.08f, 0.08f, 0.08f}));
    }

}
