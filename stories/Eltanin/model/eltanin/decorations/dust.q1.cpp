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

#include <cmath>
#include <span>

namespace eltanin::decorations {

    using namespace fqsm::api;
    using rmmr::Pose;
    using rmmr::RGB;
    using rmmr::vec3;

    namespace {

        constexpr seconds bornLife = 2.5;
        constexpr float coolTau = 0.35f;

        auto composeBox(Writing context, Pose pose, vec3 half, phys::Kelvins temperature) -> rmmr::scene::actor::Mesh::Id {
            const auto scene = with<locality::Thing>::get_global(context).scene;
            const auto& resources = with<Dust>::get_global(context).resources;
            if (not resources)
                return context.refuse("eltanin::decorations::Dust::spawn: resources not bound");
            const auto& geometry = with<rmmr::resource::geometry::Asset>::get(context, resources->scrap);
            if (geometry.entries.empty() or geometry.surfaceCatalogs.empty())
                return context.refuse("eltanin::decorations::Dust::spawn: scrap has no entry");
            const auto& catalog = geometry.surfaceCatalogs.front();
            umap<rmmr::resource::geometry::SurfaceId, rmmr::resource::material::Instance> surfaces;
            for (const auto& [name, surface] : catalog) {
                const auto albedo = name == "face" ? "panel_tech_1.bmp" : "STEEL4.JPG";
                surfaces.emplace(surface, rmmr::resource::material::Instance{.material = resources->hull, .textures = {{"albedoMap", albedo}}});
            }
            auto meshQuantum = with<rmmr::scene::actor::Mesh>::compose(context, rmmr::resource::meshpack::Asset::Resolved{.geometry = resources->scrap, .entry = rmmr::resource::geometry::EntryId{0}, .surfaces = std::move(surfaces), .texpack = resources->mech});
            if (not meshQuantum)
                return context.refuse("eltanin::decorations::Dust::spawn: mesh compose failed");
            const vec3 scale{half.x * 2.0f, half.y * 2.0f, half.z * 2.0f};
            const auto actor = with<rmmr::scene::Interface>::createMeshActor(context, scene, pose, std::move(*meshQuantum), with<rmmr::scene::actor::MeshState>::defaults(RGB{1.0f, 1.0f, 1.0f}, 1.0f, scale));
            const float intact[] = {1.0f};
            with<rmmr::scene::actor::Mesh>::writeCohesions(context, actor, std::span<const float>{intact, 1});
            const float kelvin[] = {temperature};
            with<rmmr::scene::actor::Mesh>::writeHeats(context, actor, std::span<const float>{kelvin, 1});
            return actor;
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
        const auto hull = with<rmmr::resource::Assets>::find<rmmr::resource::material::Asset>(context, rmmr::resource::Unit::Name::from("Eltanin", "hull"));
        if (not hull) {
            context.refuse("eltanin::decorations::Dust::bindResources: hull material missing");
            return;
        }
        const auto mech = with<rmmr::resource::Assets>::find<rmmr::resource::texpack::Pack>(context, rmmr::resource::Unit::Name::from("Eltanin", "mech"));
        if (not mech) {
            context.refuse("eltanin::decorations::Dust::bindResources: mech texpack missing");
            return;
        }
        with<Dust>::modify_global(context)->resources = Resources{.scrap = *scrap, .hull = *hull, .mech = *mech};
    }

    void Dust::Actions::update(Writing context, seconds dt) {
        if (dt <= 0)
            return;
        const auto scene = with<locality::Thing>::get_global(context).scene;
        const float ambient = with<rmmr::scene::Root>::exists(context, scene) ? with<rmmr::scene::Root>::get(context, scene).atmosphereTemperature : phys::Settings::skyKelvin;
        const float fade = std::exp(-float(dt) / coolTau);
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
            dust->temperature = ambient + (dust->temperature - ambient) * fade;
            dust->life -= dt;
            if (with<rmmr::scene::actor::Mesh>::exists(context, id)) {
                const float kelvin[] = {dust->temperature};
                with<rmmr::scene::actor::Mesh>::writeHeats(context, id, std::span<const float>{kelvin, 1});
            }
            if (dust->life <= 0)
                gone.push_back(id);
        }
        for (const auto id : gone)
            dropActor(context, id);
    }

    auto Dust::Actions::spawn(Writing context, Pose pose, vec3 half, vec3 linear, vec3 omega, phys::Kelvins temperature) -> Id {
        const vec3 size = glm::max(half, vec3{0.08f, 0.08f, 0.08f});
        const auto actor = composeBox(context, pose, size, temperature);
        if (not with<rmmr::scene::Node>::exists(context, actor))
            return actor;
        with<Dust>::extend(context, actor, Dust::Quantum{.linear = linear, .omega = omega, .temperature = temperature, .half = size, .life = bornLife});
        return actor;
    }

}
