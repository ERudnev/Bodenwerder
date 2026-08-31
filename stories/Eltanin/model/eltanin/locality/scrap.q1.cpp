#include <eltanin/locality/scrap.q1.h>

#include <eltanin/physics/compound.q1.h>
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

#include <span>

namespace eltanin::locality {

    using namespace fqsm::api;
    using namespace rmmr;

    namespace {

        constexpr float minHalf = 0.25f;
        constexpr float vaporAt = 4.0f;
        constexpr float cutStep = 0.5f;
        constexpr int maxCuts = 3;
        constexpr float bornCohesion = 0.5f;
        constexpr float splitPop = 2.0f;
        constexpr float heatUploadStep = 10.0f;

        auto linearOf(const phys::Body::Quantum& body, const phys::rigid::Solid::Quantum& solid) -> vec3 {
            return vec3{(body.position - solid.center.prev) / phys::Settings::fixedStep};
        }

        struct Box {
            vec3 center;
            quat rotation;
            vec3 half;
        };

        auto longestAxis(vec3 half) -> int {
            if (half.y > half.x and half.y >= half.z)
                return 1;
            if (half.z > half.x and half.z >= half.y)
                return 2;
            return 0;
        }

        auto cutCount(float cohesion) -> int {
            if (cohesion < -vaporAt)
                return -1;
            if (cohesion > 0.0f)
                return 0;
            return glm::min(maxCuts, 1 + static_cast<int>(-cohesion / cutStep));
        }

        void dichotomy(const Box& box, int cuts, vector<Box>& out) {
            if (cuts <= 0) {
                out.push_back(box);
                return;
            }
            const int axis = longestAxis(box.half);
            if (box.half[axis] < minHalf * 2.0f) {
                out.push_back(box);
                return;
            }
            vec3 unit{0.0f, 0.0f, 0.0f};
            unit[axis] = 1.0f;
            const vec3 along = box.rotation * unit;
            const float h = box.half[axis] * 0.5f;
            Box first = box;
            first.half[axis] = h;
            first.center = box.center - along * h;
            Box second = box;
            second.half[axis] = h;
            second.center = box.center + along * h;
            dichotomy(first, cuts - 1, out);
            dichotomy(second, cuts - 1, out);
        }

    }

    void Scrap::Actions::bindResources(Writing context) {
        if (with<Scrap>::get_global(context).resources)
            return;
        const auto scrap = with<resource::Assets>::find<resource::geometry::Asset>(context, resource::Unit::Name::from("Eltanin", "scrap"));
        if (not scrap) {
            context.refuse("eltanin::locality::Scrap::bindResources: scrap geometry missing");
            return;
        }
        const auto hull = with<resource::Assets>::find<resource::material::Asset>(context, resource::Unit::Name::from("Eltanin", "hull"));
        if (not hull) {
            context.refuse("eltanin::locality::Scrap::bindResources: hull material missing");
            return;
        }
        const auto mech = with<resource::Assets>::find<resource::texpack::Pack>(context, resource::Unit::Name::from("Eltanin", "mech"));
        if (not mech) {
            context.refuse("eltanin::locality::Scrap::bindResources: mech texpack missing");
            return;
        }
        with<Scrap>::modify_global(context)->resources = Resources{.scrap = *scrap, .hull = *hull, .mech = *mech};
    }

    void Scrap::Actions::update(Writing context) {
        vector<Id> living;
        for (auto [id, _] : context->aspect<Scrap>().items())
            living.push_back(id);
        for (const auto id : living) {
            if (not with<Scrap>::exists(context, id))
                continue;
            const auto& scrap = with<Scrap>::get(context, id);
            if (not with<phys::rigid::Solid>::exists(context, scrap.body) or not with<phys::Body>::exists(context, scrap.body))
                continue;
            const auto& solid = with<phys::rigid::Solid>::get(context, scrap.body);
            const auto& body = with<phys::Body>::get(context, scrap.body);
            if (solid.center.temperature >= phys::Settings::scrapVaporKelvin) {
                with<Scrap>::kraken(context, id);
                continue;
            }
            const int cuts = cutCount(solid.center.cohesion);
            if (cuts == 0)
                continue;
            if (cuts > 0) {
                const int axis = longestAxis(solid.halfExtents);
                if (solid.halfExtents[axis] >= minHalf * 2.0f)
                    breakOff(context, vec3{body.position}, body.orientation, solid.halfExtents, body.totalMass, linearOf(body, solid), solid.center.cohesion, solid.center.temperature);
            }
            with<Scrap>::kraken(context, id);
        }
    }

    auto Scrap::Actions::spawn(Writing context, Pose pose, vec3 halfExtents, float mass, vec3 linear, vec3 omega, float cohesion, phys::Kelvins temperature) -> Id {
        const auto scene = with<Thing>::get_global(context).scene;
        const vec3 half = glm::max(halfExtents, vec3{0.08f, 0.08f, 0.08f});
        if (mass <= 0.0f)
            return context.refuse("eltanin::locality::Scrap::spawn: mass must be positive");
        const auto& resources = with<Scrap>::get_global(context).resources;
        if (not resources)
            return context.refuse("eltanin::locality::Scrap::spawn: resources not bound");
        const auto& geometry = with<resource::geometry::Asset>::get(context, resources->scrap);
        if (geometry.entries.empty() or geometry.surfaceCatalogs.empty())
            return context.refuse("eltanin::locality::Scrap::spawn: scrap has no entry");
        const auto& catalog = geometry.surfaceCatalogs.front();
        umap<resource::geometry::SurfaceId, resource::material::Instance> surfaces;
        for (const auto& [name, surface] : catalog) {
            const auto albedo = name == "face" ? "panel_tech_1.bmp" : "STEEL4.JPG";
            surfaces.emplace(surface, resource::material::Instance{.material = resources->hull, .textures = {{"albedoMap", albedo}}});
        }
        auto meshQuantum = with<scene::actor::Mesh>::compose(context, resource::meshpack::Asset::Resolved{.geometry = resources->scrap, .entry = resource::geometry::EntryId{0}, .surfaces = std::move(surfaces), .texpack = resources->mech});
        if (not meshQuantum)
            return context.refuse("eltanin::locality::Scrap::spawn: mesh compose failed");
        const vec3 scale{half.x * 2.0f, half.y * 2.0f, half.z * 2.0f};
        const auto actor = with<scene::Interface>::createMeshActor(context, scene, pose, std::move(*meshQuantum), with<scene::actor::MeshState>::defaults(RGB{1.0f, 1.0f, 1.0f}, 1.0f, scale));
        // Hull wreck-mix is actor-wide; cohesion 0 would paint the steel rims with panel_tech_1.
        const float intact[] = {1.0f};
        with<scene::actor::Mesh>::writeCohesions(context, actor, std::span<const float>{intact, 1});
        const float kelvin[] = {temperature};
        with<scene::actor::Mesh>::writeHeats(context, actor, std::span<const float>{kelvin, 1});
        const float radius = glm::length(half);
        const auto body = with<phys::Body>::create(context, phys::Body::Quantum{.position = dvec3{pose.position}, .orientation = pose.rotation, .totalMass = mass, .radius = radius});
        quat prevOri = pose.rotation;
        const float omegaLen = glm::length(omega);
        if (omegaLen > 1.0e-12f) {
            const quat step = glm::angleAxis(-omegaLen * float(phys::Settings::fixedStep), omega / omegaLen);
            prevOri = glm::normalize(step * pose.rotation);
        }
        with<phys::rigid::Solid>::extend(context, body, phys::rigid::Solid::Quantum{
            .center = phys::Particle{phys::Matter{.position = dvec3{pose.position}, .mass = mass, .temperature = temperature, .cohesion = cohesion}, dvec3{pose.position} - dvec3{linear * float(phys::Settings::fixedStep)}, vec3{0.0f, 0.0f, 0.0f}},
            .prevOri = prevOri,
            .forceAngular = vec3{0.0f, 0.0f, 0.0f},
            .kind = phys::rigid::Solid::Kind::box,
            .halfExtents = half,
        });
        with<phys::Compound>::extend(context, body, phys::Compound::Quantum{.members = {}});
        const auto thing = with<Thing>::create(context, Thing::Quantum{.bornAt = with<Thing>::get_global(context).now});
        with<Scrap>::extend(context, thing, Scrap::Quantum{.body = body, .actor = actor, .gpuKelvin = temperature});
        return thing;
    }

    void Scrap::Actions::breakOff(Writing context, vec3 worldCenter, quat worldRot, vec3 halfExtents, float mass, vec3 linear, float cohesion, phys::Kelvins temperature) {
        if (temperature >= phys::Settings::scrapVaporKelvin)
            return;
        const int cuts = cutCount(cohesion);
        if (cuts < 0 or mass <= 0.0f)
            return;
        Box seed{.center = worldCenter, .rotation = glm::normalize(worldRot), .half = glm::max(halfExtents, vec3{0.08f, 0.08f, 0.08f})};
        vector<Box> pieces;
        dichotomy(seed, cuts, pieces);
        if (pieces.empty())
            return;
        const float pieceMass = mass / static_cast<float>(pieces.size());
        const vec3 omega{0.0f, 0.0f, 0.0f};
        for (const Box& piece : pieces) {
            const vec3 offset = piece.center - worldCenter;
            const float offsetLen = glm::length(offset);
            const vec3 pop = offsetLen > 1.0e-5f ? (offset / offsetLen) * splitPop : vec3{0.0f, 0.0f, 0.0f};
            spawn(context, Pose{.position = piece.center, .rotation = piece.rotation}, piece.half, pieceMass, linear + pop, omega, bornCohesion, temperature);
        }
    }

    void Scrap::Actions::followBody(Stewarding context) {
        auto nodes = context.direct<scene::Node>();
        auto bodies = context.direct<phys::Body>();
        auto solids = context.direct<phys::rigid::Solid>();
        for (auto [_, scrap] : context.direct<Scrap>().items) {
            auto* node = nodes.items.find(scrap.actor);
            if (not node)
                continue;
            auto* body = bodies.items.find(scrap.body);
            if (not body)
                continue;
            node->pose = body->pose();
            auto* solid = solids.items.find(scrap.body);
            if (not solid)
                continue;
            const float kelvin = solid->center.temperature;
            if (glm::abs(kelvin - scrap.gpuKelvin) < heatUploadStep)
                continue;
            scrap.gpuKelvin = kelvin;
            const float heats[] = {kelvin};
            with<scene::actor::Mesh>::writeHeats(context, scrap.actor, std::span<const float>{heats, 1});
        }
    }

    auto Scrap::customAspectReactions() -> const Behavior {
        return {
            reaction::structural::custody<Scrap, rmmr::scene::actor::Mesh, &Scrap::Quantum::actor>{},
            reaction::structural::custody<Scrap, phys::rigid::Solid, &Scrap::Quantum::body>{},
        };
    }

}
